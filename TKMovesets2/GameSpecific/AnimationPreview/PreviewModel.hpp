#pragma once
#include "GameTypes.h"
#include <array>
#include <span>
#include <string>
#include <vector>

// Animation preview: the mannequin model (a body built from scratch by tools/build_preview_mannequin.py, on the
// skeleton of BlenderTekkenDirect's TekkenMannequin_no_ik.blend, the bones the animation drives), its pose from one frame of T7 body channels and
// CPU skinning. No graphics API here: AnimationPreview (Layouts/EditorVisuals) draws the result.
namespace AnimPreview {
// Row-major 4x4 matrix, column vectors (p' = M p), as Blender's Matrix.
struct Mat4 {
    std::array<float, 16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float& operator()(int r, int c) { return m[r * 4 + c]; }
    float operator()(int r, int c) const { return m[r * 4 + c]; }
    static Mat4 Translation(float x, float y, float z);
    Mat4 operator*(const Mat4& other) const;
    // Inverse of an affine matrix (rotation, scale and translation).
    Mat4 AffineInverse() const;
    std::array<float, 3> Point(const std::array<float, 3>& p) const;
    std::array<float, 3> Vector(const std::array<float, 3>& v) const;
};

// Blender pose-bone rotation modes, in the exporter's order.
enum class RotationMode : uint8_t { XYZ, XZY, YXZ, YZX, ZXY, ZYX, Quaternion };
// Blender's Euler -> matrix for a rotation mode ("XYZ" applies X first: Rz * Ry * Rx).
Mat4 EulerMatrix(float x, float y, float z, RotationMode mode);

class Model {
public:
    struct Bone {
        std::string name;
        int parent = -1;
        RotationMode mode = RotationMode::XYZ;
        uint8_t flags = 0;
        Mat4 rest, restInverse;
        Mat4 local; // parent rest^-1 * rest (rest itself for a root)
    };
    struct Vertex {
        std::array<float, 3> position, normal;
        std::array<uint16_t, 4> bones{}; // skin bone indices
        std::array<float, 4> weights{};
    };
    struct Material { uint32_t first = 0, count = 0; std::array<float, 3> colour{}; };

    // Parses the exporter's TKMP layout; throws std::runtime_error on malformed data.
    static Model Load(std::span<const Byte> bytes);
    // The model embedded in the executable (AnimPreviewResources.c), loaded once. Throws when it is missing or malformed.
    static const Model& Embedded();

    std::vector<Bone> bones;
    std::vector<uint16_t> skin;  // skin bone -> bone
    std::vector<int> order;      // bones, parents first
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<Material> materials;
    int Find(std::string_view name) const;
};

// The 23 standard T7 body channels of one frame, three floats each (0xC8 form: positions in game units, rotations as
// the 0xC8 Euler triples).
using Channels = std::array<float, 69>;

// Linear interpolation between two frames (t in [0, 1]): positions (channels 0 to 2) directly, rotations along the
// shorter way round so that an angle wrapping past +-pi does not spin the bone.
Channels Interpolate(const Channels& a, const Channels& b, float t);

struct PoseOptions {
    // Root motion (channel 0, GlobalPos) moves the body; off keeps the character over the origin (LocalPos only).
    bool rootMotion = true;
};
// Bone matrices (armature space) for one frame, with BlenderTekkenDirect's applyRotationFromAnimdata mapping from T7
// channels to the mannequin's pose bones. Every other bone keeps its rest pose relative to its parent.
std::vector<Mat4> Pose(const Model& model, const Channels& channels, const PoseOptions& options = {});
// Linear blend skinning of every vertex (positions and normals, armature space: Y up; posed, the character faces +X).
void Skin(const Model& model, std::span<const Mat4> pose, std::vector<float>& positions, std::vector<float>& normals);
}
