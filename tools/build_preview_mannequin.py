"""Builds the animation preview mannequin and writes it for the editor (run inside Blender).

    blender -b TekkenMannequin_no_ik.blend --python tools/build_preview_mannequin.py -- OUTPUT.bin [REFERENCE.json] [HELPER]

The mannequin is made here from scratch: an artist's-mannequin body (rounded segments and ball joints) built from code,
each part bound to one bone (the abdomen blends hip and chest). Only the rig's skeleton is read from the .blend file:
the bones that the animation drives and their parents, with their rest transforms, since these define what the T7 body
channels mean (BlenderTekkenDirect's applyRotationFromAnimdata). Nothing of the file's meshes is used.

Left limbs are warm, right limbs cool and the face has a visor band, so sides and facing read at a glance.

The output is the little-endian layout read by `AnimPreview::Model::Load`
(TKMovesets2/GameSpecific/AnimationPreview/PreviewModel.cpp):

    char[4] "TKMP", u32 version (1), u32 bones, u32 skinBones, u32 vertices, u32 triangles, u32 materials
    bones x      {u8 nameLength, name, i16 parent, u8 rotationMode, u8 flags, f32[16] rest (row-major)}
    skinBones x  u16 bone index
    vertices x   {f32 position[3], i8 normal[3], u8 pad, u16 skinBone[4], u8 weight[4]}
    materials x  {u32 firstTriangle, u32 triangleCount, u8 rgb[3], u8 pad}
    triangles x  u16[3]

Rotation modes: 0 XYZ, 1 XZY, 2 YXZ, 3 YZX, 4 ZXY, 5 ZYX, 6 quaternion. Flags: 1 inherit rotation, 2 inherit scale,
4 connected. Armature space: Y up, the character faces +Z, its left is +X.

REFERENCE.json (optional): bone matrices and evaluated vertex positions for a few T7 0xC8 frames, posed by Blender with
BlenderTekkenDirect's applyRotationFromAnimdata; the C++ pose code is checked against it (tests/AnimationPreviewTests.cpp).
"""
import bpy, bmesh, struct, sys, json, math, importlib.util
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUTPUT = argv[0]
REFERENCE = argv[1] if len(argv) > 1 else None
HELPER = argv[2] if len(argv) > 2 else "G:/Dev/Python/BlenderTekkenDirect/TekkenAnimHelper.py"

MODES = {"XYZ": 0, "XZY": 1, "YXZ": 2, "YZX": 3, "ZXY": 4, "ZYX": 5, "QUATERNION": 6, "AXIS_ANGLE": 6}
# Parents first.
BONES = ["MODEL_00", "BODY_SCALE__group", "BASE", "Spine1", "Spine2", "Neck", "Head",
         "L_Shoulder", "L_Arm", "L_ForeArm", "L_Hand", "R_Shoulder", "R_Arm", "R_ForeArm", "R_Hand",
         "Hip", "L_UpLeg", "L_Leg", "L_Foot", "R_UpLeg", "R_Leg", "R_Foot"]

# --- Skeleton: rest transforms of the source rig, then a new armature with only these bones ----------------------
source = bpy.data.objects["Armature"]
skeleton = []
for name in BONES:
    b = source.data.bones[name]
    skeleton.append((name, b.parent.name if b.parent else None, b.matrix_local.copy(), b.length,
                     source.pose.bones[name].rotation_mode))
for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj)
for collection in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
    for block in list(collection):
        collection.remove(block)

arm = bpy.data.objects.new("Armature", bpy.data.armatures.new("Armature"))
bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
for name, parent, matrix, length, _ in skeleton:
    eb = arm.data.edit_bones.new(name)
    eb.head = (0, 0, 0)
    eb.tail = (0, length, 0)
    eb.matrix = matrix
    eb.use_connect = False
    if parent:
        eb.parent = arm.data.edit_bones[parent]
bpy.ops.object.mode_set(mode="OBJECT")
for name, _, _, _, mode in skeleton:
    arm.pose.bones[name].rotation_mode = mode
rest_error = max(abs(arm.data.bones[n].matrix_local[r][c] - m[r][c]) for n, _, m, _, _ in skeleton for r in range(4) for c in range(4))
assert rest_error < 1e-5, rest_error

# --- Body ------------------------------------------------------------------------------------------------------------
COLOURS = {"body": (178, 172, 162), "head": (192, 186, 176), "joint": (66, 70, 82), "visor": (60, 156, 178),
           "left": (204, 162, 136), "right": (140, 160, 200), "left_end": (176, 136, 112), "right_end": (114, 132, 172)}
verts, faces, face_mats, weights = [], [], [], []
materials = sorted(COLOURS)


def lerp(a, b, t):
    return a + (b - a) * t


def part(material, bone, p0, p1, r0, r1=None, ref=(0, 0, 1), roundness=2.6, segments=24, rings=14):
    """Rounded segment from p0 to p1 (a superellipsoid tapered from radii r0 to r1).

    r0, r1: radius or (across e1, across e2), with e1 = axis x ref and e2 = e1 x axis. `bone` is a bone name or a
    function position -> [(bone, weight)]. roundness: 2 is an ellipsoid, higher is boxier."""
    p0, p1 = Vector(p0), Vector(p1)
    r0 = (r0, r0) if isinstance(r0, (int, float)) else r0
    r1 = r0 if r1 is None else ((r1, r1) if isinstance(r1, (int, float)) else r1)
    axis = p1 - p0
    half = axis.length / 2
    axis.normalize()
    e1 = axis.cross(Vector(ref)).normalized()
    e2 = e1.cross(axis).normalized()
    centre = (p0 + p1) / 2
    e = 2 / roundness

    def signed(value, power):
        return math.copysign(abs(value) ** power, value)

    base = len(verts)
    points = []
    for i in range(1, rings):
        theta = math.pi * i / rings
        along = -signed(math.cos(theta), e)
        radial = abs(math.sin(theta)) ** e
        t = (along + 1) / 2
        rx, rz = lerp(r0[0], r1[0], t), lerp(r0[1], r1[1], t)
        for j in range(segments):
            phi = 2 * math.pi * j / segments
            points.append(centre + axis * (along * half) + e1 * (signed(math.cos(phi), e) * radial * rx)
                          + e2 * (signed(math.sin(phi), e) * radial * rz))
    points.append(centre - axis * half)
    points.append(centre + axis * half)
    verts.extend(points)
    for p in points:
        weights.append(bone(p) if callable(bone) else [(bone, 1.0)])
    bottom, top = base + len(points) - 2, base + len(points) - 1
    index = lambda i, j: base + (i - 1) * segments + j % segments
    new_faces = []
    for j in range(segments):
        new_faces.append((bottom, index(1, j + 1), index(1, j)))
        new_faces.append((top, index(rings - 1, j), index(rings - 1, j + 1)))
        for i in range(1, rings - 1):
            new_faces.append((index(i, j), index(i, j + 1), index(i + 1, j + 1), index(i + 1, j)))
    faces.extend(new_faces)
    face_mats.extend([materials.index(material)] * len(new_faces))


def ball(material, bone, centre, radius):
    c = Vector(centre)
    part(material, bone, c - Vector((0, radius, 0)), c + Vector((0, radius, 0)), radius, roundness=2, segments=18, rings=10)


def abdomen(p):
    # Hip below 1.10, chest (Spine1) above 1.28, blended in between.
    t = min(1.0, max(0.0, (p.y - 1.10) / 0.18))
    t = t * t * (3 - 2 * t)
    return [(b, w) for b, w in (("Hip", 1 - t), ("Spine1", t)) if w > 1e-4]


# Torso and head
part("body", "Hip", (0, 0.89, -0.005), (0, 1.14, 0), (0.150, 0.100), (0.158, 0.104), roundness=3.2)
part("body", abdomen, (0, 1.02, 0), (0, 1.36, 0), (0.122, 0.090), (0.130, 0.094), roundness=2.4)
part("body", "Spine2", (0, 1.24, 0.004), (0, 1.585, -0.004), (0.140, 0.098), (0.178, 0.112), roundness=3.0)
part("body", "Neck", (0, 1.50, -0.006), (0, 1.745, 0.004), 0.047, 0.043, roundness=3.0, segments=18)
part("head", "Head", (0, 1.680, 0.012), (0, 1.902, 0.0), (0.074, 0.088), (0.084, 0.098), roundness=2.3, segments=28, rings=18)
part("visor", "Head", (-0.064, 1.778, 0.064), (0.064, 1.778, 0.064), (0.020, 0.028), ref=(0, 1, 0), roundness=3.0, segments=16)
for side, sx in (("L", 1), ("R", -1)):
    limb, end = ("left", "left_end") if side == "L" else ("right", "right_end")
    x = lambda v: v * sx
    # Shoulder cap (clavicle), arm
    part(limb, f"{side}_Shoulder", (x(0.075), 1.505, -0.004), (x(0.245), 1.522, 0), 0.050, 0.060, roundness=2.4)
    ball("joint", f"{side}_Arm", (x(0.237), 1.518, 0), 0.056)
    part(limb, f"{side}_Arm", (x(0.262), 1.518, 0), (x(0.540), 1.518, 0), 0.050, 0.040, roundness=2.6)
    ball("joint", f"{side}_ForeArm", (x(0.558), 1.518, 0), 0.039)
    part(limb, f"{side}_ForeArm", (x(0.574), 1.518, 0), (x(0.782), 1.518, 0), 0.041, 0.031, roundness=2.6)
    ball("joint", f"{side}_Hand", (x(0.798), 1.517, 0), 0.030)
    part(end, f"{side}_Hand", (x(0.810), 1.513, 0.0), (x(0.972), 1.505, 0.0), (0.022, 0.044), (0.018, 0.040), roundness=4.0)
    part(end, f"{side}_Hand", (x(0.815), 1.507, 0.030), (x(0.872), 1.500, 0.074), 0.015, 0.012, ref=(0, 1, 0), segments=12)
    # Leg
    ball("joint", f"{side}_UpLeg", (x(0.1), 0.985, 0), 0.072)
    part(limb, f"{side}_UpLeg", (x(0.1), 0.965, 0), (x(0.1), 0.578, 0), 0.084, 0.057, roundness=2.6)
    ball("joint", f"{side}_Leg", (x(0.1), 0.550, 0), 0.054)
    part(limb, f"{side}_Leg", (x(0.1), 0.525, 0), (x(0.1), 0.150, 0), 0.058, 0.039, roundness=2.6)
    ball("joint", f"{side}_Foot", (x(0.1), 0.130, 0), 0.040)
    part(end, f"{side}_Foot", (x(0.1), 0.046, -0.060), (x(0.1), 0.036, 0.212), (0.048, 0.046), (0.044, 0.034),
         ref=(0, 1, 0), roundness=3.4)

mesh = bpy.data.meshes.new("Mannequin")
mesh.from_pydata([tuple(v) for v in verts], [], faces)
for name in materials:
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*[c / 255 for c in COLOURS[name]], 1)
    mesh.materials.append(mat)
for poly, m in zip(mesh.polygons, face_mats):
    poly.material_index = m
    poly.use_smooth = True
# Outward winding (and so outward normals) whatever the handedness of each part's frame.
bm = bmesh.new()
bm.from_mesh(mesh)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(mesh)
bm.free()
mesh.update()
body = bpy.data.objects.new("Mannequin", mesh)
bpy.context.scene.collection.objects.link(body)
body.parent = arm
for name in BONES:
    body.vertex_groups.new(name=name)
for i, ws in enumerate(weights):
    for bone, w in ws:
        body.vertex_groups[bone].add([i], w, "REPLACE")
body.modifiers.new("Armature", "ARMATURE").object = arm
meshes = [body]

# --- Export ----------------------------------------------------------------------------------------------------------
bones = list(arm.data.bones)
index = {b.name: i for i, b in enumerate(bones)}
arm_inv = arm.matrix_world.inverted()

skin = []          # bone indices used by vertex groups
skin_index = {}
vertices = []      # (pos, normal, [(skin, weight)])
triangles = {}     # material name -> list of (a, b, c)
for obj in meshes:
    mesh = obj.data
    to_arm = arm_inv @ obj.matrix_world
    normal_m = to_arm.to_3x3().inverted().transposed()
    groups = {}
    for g in obj.vertex_groups:
        if g.name in index:
            if g.name not in skin_index:
                skin_index[g.name] = len(skin)
                skin.append(index[g.name])
            groups[g.index] = skin_index[g.name]
    base = len(vertices)
    for v in mesh.vertices:
        p = to_arm @ v.co
        n = (normal_m @ v.normal).normalized()
        w = sorted([(groups[g.group], g.weight) for g in v.groups if g.group in groups and g.weight > 1e-5], key=lambda x: -x[1])[:4]
        vertices.append((tuple(p), tuple(n), w))
    mesh.calc_loop_triangles()
    for tri in mesh.loop_triangles:
        slot = obj.material_slots[tri.material_index] if tri.material_index < len(obj.material_slots) else None
        name = slot.material.name if slot and slot.material else ""
        triangles.setdefault(name, []).append(tuple(base + i for i in tri.vertices))

assert len(vertices) < 65536 and len(skin) < 65536
out = bytearray()
by_material = sorted(triangles.items(), key=lambda kv: kv[0])
out += b"TKMP" + struct.pack("<6I", 1, len(bones), len(skin), len(vertices), sum(len(t) for _, t in by_material), len(by_material))
for b in bones:
    pb = arm.pose.bones[b.name]
    name = b.name.encode()
    flags = (1 if b.use_inherit_rotation else 0) | (2 if b.inherit_scale == "FULL" else 0) | (4 if b.use_connect else 0)
    out += struct.pack("<B", len(name)) + name + struct.pack("<hBB", index[b.parent.name] if b.parent else -1, MODES[pb.rotation_mode], flags)
    out += struct.pack("<16f", *[b.matrix_local[r][c] for r in range(4) for c in range(4)])
for s in skin:
    out += struct.pack("<H", s)
for p, n, w in vertices:
    total = sum(x[1] for x in w) or 1.0
    q = [min(255, max(0, round(x[1] / total * 255))) for x in w]
    # Weights sum to 255 exactly: the rounding error goes to the largest weight.
    if q:
        q[0] += 255 - sum(q)
    idx = [x[0] for x in w] + [0] * (4 - len(w))
    q += [0] * (4 - len(q))
    out += struct.pack("<3f3bB4H4B", *p, *[max(-127, min(127, round(c * 127))) for c in n], 0, *idx, *q)
first = 0
for name, tris in by_material:
    out += struct.pack("<II3BB", first, len(tris), *COLOURS[name], 0)
    first += len(tris)
for _, tris in by_material:
    for t in tris:
        out += struct.pack("<3H", *t)
with open(OUTPUT, "wb") as f:
    f.write(out)
print("EXPORT", OUTPUT, len(out), "bytes", len(bones), "bones", len(skin), "skin bones", len(vertices), "vertices", first, "triangles")

if REFERENCE:
    spec = importlib.util.spec_from_file_location("TekkenAnimHelper", HELPER)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    frames = []
    # Deterministic test frames: the 69 floats of a 0xC8 frame (23 channels x 3).
    for k in range(4):
        values = [0.0] * 69
        values[3], values[4], values[5] = 40.0 * k, 1005.0 - 30 * k, -25.0 * k
        values[0], values[2] = 15.0 * k, 10.0 * k
        for i in range(9, 69):
            values[i] = math.sin(i * 1.7 + k * 0.9) * (0.6 + 0.2 * k)
        frames.append(values)
    ref = {"frames": []}
    for values in frames:
        for pb in arm.pose.bones:
            pb.location = (0, 0, 0); pb.rotation_euler = (0, 0, 0); pb.rotation_quaternion = (1, 0, 0, 0); pb.scale = (1, 1, 1)
        helper.applyRotationFromAnimdata(arm, values)
        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        entry = {"values": values, "bones": {}, "vertices": []}
        for n in BONES[1:]:
            m = arm.pose.bones[n].matrix
            entry["bones"][n] = [m[r][c] for r in range(4) for c in range(4)]
        base = 0
        for obj in meshes:
            ev = obj.evaluated_get(depsgraph)
            em = ev.to_mesh()
            to_arm = arm_inv @ obj.matrix_world
            for i in range(0, len(em.vertices), 37):
                entry["vertices"].append([base + i] + list(to_arm @ em.vertices[i].co))
            base += len(obj.data.vertices)
            ev.to_mesh_clear()
        ref["frames"].append(entry)
    with open(REFERENCE, "w") as f:
        json.dump(ref, f)
    print("REFERENCE", REFERENCE, len(ref["frames"]), "frames")
