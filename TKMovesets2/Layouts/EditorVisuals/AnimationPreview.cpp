#include <glad/glad.h>
#include <Windows.h>

#include "AnimationPreview.hpp"
#include "Localization.hpp"
#include "Settings.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

using namespace AnimPreview;

namespace {
constexpr float Speeds[] = {0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
constexpr const char* SpeedLabels[] = {"0.1x", "0.25x", "0.5x", "0.75x", "1x", "2x"};
constexpr float DefaultHeight = 340, MinHeight = 160;
float g_lastHeight = DefaultHeight; // shared by the forms: a resized preview keeps its size in the next form
// Vertical field of view: 0.62 rad, widened in views narrower than 1.3:1 so that the horizontal view stays the same.
float FieldOfView(float aspect) {
    constexpr float Fov = 0.62f, MinAspect = 1.3f;
    return aspect >= MinAspect ? Fov : 2 * std::atan(std::tan(Fov / 2) * MinAspect / aspect);
}
// Form width from which the preview goes beside the fields (each gets half).
constexpr float SideBySideWidth = 1000;
constexpr const char* OpenSettingKey = "editor.animation_preview_open";
// Open or collapsed, as last chosen in any form: the state new forms start in.
bool& LastOpen() { static bool open = Settings::Get(OpenSettingKey, true); return open; }
// Playback options (speed and the boxes), as last chosen in any form: the options new forms start with. Saved in the
// settings. Captures (synchronous) start with the defaults and save nothing, so that they do not depend on the user's.
constexpr const char* SpeedSettingKey = "editor.animation_preview_speed_v2"; // index in Speeds (v2: with 0.75x)
constexpr const char* LoopSettingKey = "editor.animation_preview_loop";
constexpr const char* RootMotionSettingKey = "editor.animation_preview_root_motion";
constexpr const char* FollowSettingKey = "editor.animation_preview_follow";
constexpr const char* DisplacementSettingKey = "editor.animation_preview_displacement";
constexpr const char* HitboxesSettingKey = "editor.animation_preview_hitboxes";
constexpr const char* OpponentSettingKey = "editor.animation_preview_opponent";
constexpr const char* OpponentAirborneSettingKey = "editor.animation_preview_opponent_airborne";
constexpr const char* OpponentAirMoveSettingKey = "editor.animation_preview_opponent_air_move";
constexpr const char* OpponentCounterhitSettingKey = "editor.animation_preview_opponent_counterhit";
constexpr const char* OpponentFacingSettingKey = "editor.animation_preview_opponent_facing";
constexpr const char* OpponentPushbackSettingKey = "editor.animation_preview_opponent_pushback";
struct PlaybackOptions {
    int speed = 4; // 1x
    bool loop = true, rootMotion = true, follow = true, displacements = true, hitboxes = true;
    bool opponent = false, opponentAirborne = false, opponentCounterhit = false, opponentPushback = true;
    int opponentAirMove = -1, opponentFacing = 0;
    bool operator==(const PlaybackOptions&) const = default;
};
PlaybackOptions& LastOptions() {
    static PlaybackOptions options = [] {
        PlaybackOptions o;
        o.speed = std::clamp(Settings::Get(SpeedSettingKey, o.speed), 0, int(IM_ARRAYSIZE(SpeedLabels)) - 1);
        o.loop = Settings::Get(LoopSettingKey, o.loop);
        o.rootMotion = Settings::Get(RootMotionSettingKey, o.rootMotion);
        o.follow = Settings::Get(FollowSettingKey, o.follow);
        o.displacements = Settings::Get(DisplacementSettingKey, o.displacements);
        o.hitboxes = Settings::Get(HitboxesSettingKey, o.hitboxes);
        o.opponent = Settings::Get(OpponentSettingKey, o.opponent);
        o.opponentAirborne = Settings::Get(OpponentAirborneSettingKey, o.opponentAirborne);
        o.opponentAirMove = Settings::Get(OpponentAirMoveSettingKey, o.opponentAirMove);
        o.opponentCounterhit = Settings::Get(OpponentCounterhitSettingKey, o.opponentCounterhit);
        o.opponentFacing = std::clamp(Settings::Get(OpponentFacingSettingKey, o.opponentFacing), 0, 3);
        o.opponentPushback = Settings::Get(OpponentPushbackSettingKey, o.opponentPushback);
        return o;
    }();
    return options;
}
void RememberOptions(const PlaybackOptions& options) {
    auto& last = LastOptions();
    if (options == last) return;
    if (options.speed != last.speed) Settings::Set(SpeedSettingKey, options.speed);
    if (options.loop != last.loop) Settings::Set(LoopSettingKey, options.loop);
    if (options.rootMotion != last.rootMotion) Settings::Set(RootMotionSettingKey, options.rootMotion);
    if (options.follow != last.follow) Settings::Set(FollowSettingKey, options.follow);
    if (options.displacements != last.displacements) Settings::Set(DisplacementSettingKey, options.displacements);
    if (options.hitboxes != last.hitboxes) Settings::Set(HitboxesSettingKey, options.hitboxes);
    if (options.opponent != last.opponent) Settings::Set(OpponentSettingKey, options.opponent);
    if (options.opponentAirborne != last.opponentAirborne) Settings::Set(OpponentAirborneSettingKey, options.opponentAirborne);
    if (options.opponentAirMove != last.opponentAirMove) Settings::Set(OpponentAirMoveSettingKey, options.opponentAirMove);
    if (options.opponentCounterhit != last.opponentCounterhit) Settings::Set(OpponentCounterhitSettingKey, options.opponentCounterhit);
    if (options.opponentFacing != last.opponentFacing) Settings::Set(OpponentFacingSettingKey, options.opponentFacing);
    if (options.opponentPushback != last.opponentPushback) Settings::Set(OpponentPushbackSettingKey, options.opponentPushback);
    last = options;
}

using Vec3 = std::array<float, 3>;
Vec3 Sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 Cross(const Vec3& a, const Vec3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
float Dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Normalize(const Vec3& a) { const float l = std::sqrt(Dot(a, a)); return l > 1e-8f ? Vec3{a[0] / l, a[1] / l, a[2] / l} : Vec3{0, 0, 1}; }

Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = Normalize(Sub(target, eye)), s = Normalize(Cross(f, up)), u = Cross(s, f);
    Mat4 m;
    m(0,0) = s[0]; m(0,1) = s[1]; m(0,2) = s[2]; m(0,3) = -Dot(s, eye);
    m(1,0) = u[0]; m(1,1) = u[1]; m(1,2) = u[2]; m(1,3) = -Dot(u, eye);
    m(2,0) = -f[0]; m(2,1) = -f[1]; m(2,2) = -f[2]; m(2,3) = Dot(f, eye);
    return m;
}
Mat4 Perspective(float fovY, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovY / 2);
    Mat4 m;
    m(0,0) = f / aspect; m(1,1) = f;
    m(2,2) = (farZ + nearZ) / (nearZ - farZ); m(2,3) = 2 * farZ * nearZ / (nearZ - farZ);
    m(3,2) = -1; m(3,3) = 0;
    return m;
}

const char* VertexShader = R"(#version 130
in vec3 aPos;
in vec3 aNormal;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec3 vPos;
void main() { vNormal = aNormal; vPos = aPos; gl_Position = uViewProj * vec4(aPos, 1.0); }
)";
const char* FragmentShader = R"(#version 130
in vec3 vNormal;
in vec3 vPos;
uniform vec4 uColour;
uniform int uLit;
uniform vec3 uEye;
out vec4 fragColour;
void main() {
    if (uLit == 0) { fragColour = uColour; return; }
    if (uLit == 2) {
        // Ground: fades out with the distance to the focus point (uEye holds the focus here).
        float d = length(vPos.xz - uEye.xz);
        fragColour = vec4(uColour.rgb, uColour.a * clamp(1.0 - d / 7.0, 0.0, 1.0));
        return;
    }
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 key = normalize(vec3(0.45, 0.85, 0.55));
    vec3 fill = normalize(vec3(-0.6, 0.35, -0.55));
    vec3 v = normalize(uEye - vPos);
    float hemi = 0.5 + 0.5 * n.y;
    vec3 base = uColour.rgb;
    vec3 c = base * (0.18 + 0.22 * hemi) + base * 0.78 * max(dot(n, key), 0.0) + base * 0.22 * max(dot(n, fill), 0.0);
    vec3 h = normalize(key + v);
    c += vec3(0.10) * pow(max(dot(n, h), 0.0), 28.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    c += vec3(0.30, 0.42, 0.65) * rim * 0.45;
    fragColour = vec4(pow(c, vec3(1.0 / 1.1)), uColour.a);
}
)";

// GL calls need a current context with the 3.0 entry points loaded (the GUI's); headless form tests have none.
bool GlAvailable() { return wglGetCurrentContext() != nullptr && glGenFramebuffers != nullptr && glCreateShader != nullptr; }

struct GlState {
    GLint framebuffer = 0, program = 0, vao = 0, arrayBuffer = 0, viewport[4]{}, scissor[4]{};
    GLboolean depth = 0, cull = 0, blend = 0, scissorTest = 0, multisample = 0;
    GLint blendSrc = 0, blendDst = 0;
    GLfloat clear[4]{};
    GlState() {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer); glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao); glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_VIEWPORT, viewport); glGetIntegerv(GL_SCISSOR_BOX, scissor);
        depth = glIsEnabled(GL_DEPTH_TEST); cull = glIsEnabled(GL_CULL_FACE); blend = glIsEnabled(GL_BLEND);
        scissorTest = glIsEnabled(GL_SCISSOR_TEST); multisample = glIsEnabled(GL_MULTISAMPLE);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrc); glGetIntegerv(GL_BLEND_DST_RGB, &blendDst);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    }
    ~GlState() {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); glUseProgram(program); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]); glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        auto set = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
        set(GL_DEPTH_TEST, depth); set(GL_CULL_FACE, cull); set(GL_BLEND, blend); set(GL_SCISSOR_TEST, scissorTest); set(GL_MULTISAMPLE, multisample);
        glBlendFunc(blendSrc, blendDst);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
    }
};
}

// Triangles (position + normal, interleaved) of a capsule from a to b: two hemispheres joined by a cylinder.
static void AppendCapsule(std::vector<float>& out, const HitShape& shape) {
    constexpr int Segments = 18, Rings = 12; // rings over the whole sphere, split at the equator
    Vec3 axis{shape.b[0] - shape.a[0], shape.b[1] - shape.a[1], shape.b[2] - shape.a[2]};
    const float length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    axis = length > 1e-6f ? Vec3{axis[0] / length, axis[1] / length, axis[2] / length} : Vec3{0, 1, 0};
    const Vec3 helper = std::fabs(axis[1]) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 u = Normalize(Cross(axis, helper)), v = Cross(axis, u);
    auto point = [&](int ring, int segment, Vec3& normal) {
        // ring 0 = the pole at a (pointing away from b), Rings = the pole at b.
        const float theta = std::numbers::pi_v<float> * ring / Rings, phi = 2 * std::numbers::pi_v<float> * segment / Segments;
        const float along = -std::cos(theta), around = std::sin(theta);
        for (int i = 0; i < 3; ++i) normal[i] = axis[i] * along + (u[i] * std::cos(phi) + v[i] * std::sin(phi)) * around;
        const auto& base = ring * 2 <= Rings ? shape.a : shape.b;
        return Vec3{base[0] + normal[0] * shape.radius, base[1] + normal[1] * shape.radius, base[2] + normal[2] * shape.radius};
    };
    auto push = [&](int ring, int segment) {
        Vec3 n; const Vec3 p = point(ring, segment, n);
        out.insert(out.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
    };
    for (int r = 0; r < Rings; ++r)
        for (int s = 0; s < Segments; ++s) {
            push(r, s); push(r + 1, s); push(r + 1, s + 1);
            push(r, s); push(r + 1, s + 1); push(r, s + 1);
        }
    // Cylinder between the hemispheres (the equator ring at a and at b).
    if (length > 1e-6f)
        for (int s = 0; s < Segments; ++s) {
            Vec3 n0, n1;
            const int e = Rings / 2;
            const Vec3 a0 = point(e, s, n0), a1 = point(e, s + 1, n1);
            const Vec3 b0{a0[0] + axis[0] * length, a0[1] + axis[1] * length, a0[2] + axis[2] * length};
            const Vec3 b1{a1[0] + axis[0] * length, a1[1] + axis[1] * length, a1[2] + axis[2] * length};
            for (const auto& [p, n] : {std::pair{a0, n0}, std::pair{b0, n0}, std::pair{b1, n1}, std::pair{a0, n0}, std::pair{b1, n1}, std::pair{a1, n1}})
                out.insert(out.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
        }
}

struct AnimationPreview::Gpu {
    bool failed = false, ready = false;
    std::string error;
    GLuint program = 0, meshVao = 0, positionVbo = 0, normalVbo = 0, indexVbo = 0;
    GLuint linesVao = 0, linesVbo = 0, planeVao = 0, planeVbo = 0, discVao = 0, discVbo = 0, hitVao = 0, hitVbo = 0;
    GLuint opponentVao = 0, opponentPositionVbo = 0, opponentNormalVbo = 0; // the opponent's mesh (same indices)
    GLsizei gridLines = 0, majorLines = 0, discVertices = 0;
    GLint uViewProj = -1, uColour = -1, uLit = -1, uEye = -1;
    // Multisampled target, resolved into `texture` (what ImGui draws).
    GLuint msaaFbo = 0, msaaColour = 0, msaaDepth = 0, resolveFbo = 0, texture = 0, resolveDepth = 0;
    int width = 0, height = 0, samples = 0;

    static GLuint Compile(GLenum type, const char* source) {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
        GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]{}; glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log); glDeleteShader(shader);
            throw std::runtime_error(std::string("Preview shader: ") + log);
        }
        return shader;
    }
    void Init() {
        const auto& model = Model::Embedded();
        const GLuint vs = Compile(GL_VERTEX_SHADER, VertexShader), fs = Compile(GL_FRAGMENT_SHADER, FragmentShader);
        program = glCreateProgram();
        glAttachShader(program, vs); glAttachShader(program, fs);
        glBindAttribLocation(program, 0, "aPos"); glBindAttribLocation(program, 1, "aNormal");
        glBindFragDataLocation(program, 0, "fragColour");
        glLinkProgram(program); glDeleteShader(vs); glDeleteShader(fs);
        GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) { char log[1024]{}; glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log); throw std::runtime_error(std::string("Preview shader link: ") + log); }
        uViewProj = glGetUniformLocation(program, "uViewProj"); uColour = glGetUniformLocation(program, "uColour");
        uLit = glGetUniformLocation(program, "uLit"); uEye = glGetUniformLocation(program, "uEye");

        glGenVertexArrays(1, &meshVao); glBindVertexArray(meshVao);
        glGenBuffers(1, &positionVbo); glBindBuffer(GL_ARRAY_BUFFER, positionVbo);
        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * 12, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        glGenBuffers(1, &normalVbo); glBindBuffer(GL_ARRAY_BUFFER, normalVbo);
        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * 12, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        glGenBuffers(1, &indexVbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * 2, model.indices.data(), GL_STATIC_DRAW);
        glGenVertexArrays(1, &opponentVao); glBindVertexArray(opponentVao);
        glGenBuffers(1, &opponentPositionVbo); glBindBuffer(GL_ARRAY_BUFFER, opponentPositionVbo);
        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * 12, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        glGenBuffers(1, &opponentNormalVbo); glBindBuffer(GL_ARRAY_BUFFER, opponentNormalVbo);
        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * 12, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo);

        // Floor: a grid every 0.5 m over 12 m, the lines through the origin brighter.
        std::vector<float> lines;
        auto line = [&](float x0, float z0, float x1, float z1) { lines.insert(lines.end(), {x0, 0.001f, z0, x1, 0.001f, z1}); };
        for (int i = -12; i <= 12; ++i) if (i) { line(i * 0.5f, -6, i * 0.5f, 6); line(-6, i * 0.5f, 6, i * 0.5f); }
        gridLines = GLsizei(lines.size() / 3);
        line(0, -6, 0, 6); line(-6, 0, 6, 0);
        majorLines = GLsizei(lines.size() / 3) - gridLines;
        glGenVertexArrays(1, &linesVao); glBindVertexArray(linesVao);
        glGenBuffers(1, &linesVbo); glBindBuffer(GL_ARRAY_BUFFER, linesVbo);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * 4, lines.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        glVertexAttrib3f(1, 0, 1, 0);

        const float plane[] = {-8, 0, -8, 8, 0, -8, 8, 0, 8, -8, 0, -8, 8, 0, 8, -8, 0, 8};
        glGenVertexArrays(1, &planeVao); glBindVertexArray(planeVao);
        glGenBuffers(1, &planeVbo); glBindBuffer(GL_ARRAY_BUFFER, planeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(plane), plane, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);

        // Unit disc (triangle fan), scaled and moved under the character as its shadow.
        std::vector<float> disc{0, 0.002f, 0};
        for (int i = 0; i <= 32; ++i) { const float a = i * 2 * std::numbers::pi_v<float> / 32; disc.insert(disc.end(), {std::cos(a), 0.002f, std::sin(a)}); }
        discVertices = GLsizei(disc.size() / 3);
        glGenVertexArrays(1, &discVao); glBindVertexArray(discVao);
        glGenBuffers(1, &discVbo); glBindBuffer(GL_ARRAY_BUFFER, discVbo);
        glBufferData(GL_ARRAY_BUFFER, disc.size() * 4, disc.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
        // Hitbox shapes: interleaved position + normal, rewritten each posed frame.
        glGenVertexArrays(1, &hitVao); glBindVertexArray(hitVao);
        glGenBuffers(1, &hitVbo); glBindBuffer(GL_ARRAY_BUFFER, hitVbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, nullptr);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void*>(12));
        glBindVertexArray(0);
        GLint maxSamples = 0; glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        samples = (std::min)(4, int(maxSamples));
        ready = true;
    }
    void ReleaseTargets() {
        if (msaaFbo) glDeleteFramebuffers(1, &msaaFbo);
        if (resolveFbo) glDeleteFramebuffers(1, &resolveFbo);
        if (msaaColour) glDeleteRenderbuffers(1, &msaaColour);
        if (msaaDepth) glDeleteRenderbuffers(1, &msaaDepth);
        if (resolveDepth) glDeleteRenderbuffers(1, &resolveDepth);
        if (texture) glDeleteTextures(1, &texture);
        msaaFbo = resolveFbo = msaaColour = msaaDepth = resolveDepth = texture = 0;
        width = height = 0;
    }
    void Resize(int w, int h) {
        if (w == width && h == height) return;
        ReleaseTargets();
        width = w; height = h;
        glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &resolveFbo); glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glGenRenderbuffers(1, &resolveDepth); glBindRenderbuffer(GL_RENDERBUFFER, resolveDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, resolveDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Preview framebuffer is incomplete");
        if (samples > 1) {
            glGenFramebuffers(1, &msaaFbo); glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo);
            glGenRenderbuffers(1, &msaaColour); glBindRenderbuffer(GL_RENDERBUFFER, msaaColour);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColour);
            glGenRenderbuffers(1, &msaaDepth); glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepth);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                // No multisampling then: draw straight into the texture.
                glDeleteFramebuffers(1, &msaaFbo); glDeleteRenderbuffers(1, &msaaColour); glDeleteRenderbuffers(1, &msaaDepth);
                msaaFbo = msaaColour = msaaDepth = 0; samples = 0;
            }
        }
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    ~Gpu() {
        if (!GlAvailable()) return; // context already gone (process exit): the driver frees everything
        ReleaseTargets();
        GLuint buffers[] = {positionVbo, normalVbo, indexVbo, linesVbo, planeVbo, discVbo, hitVbo, opponentPositionVbo, opponentNormalVbo};
        GLuint arrays[] = {meshVao, linesVao, planeVao, discVao, hitVao, opponentVao};
        glDeleteBuffers(9, buffers); glDeleteVertexArrays(6, arrays);
        if (program) glDeleteProgram(program);
    }
};

AnimationPreview::AnimationPreview() : m_open(LastOpen()) {
    PlaybackOptions options;
    if (synchronous) {
        options.displacements = defaultDisplacements;
        options.opponent = defaultOpponent; options.opponentAirborne = defaultOpponentAirborne;
        options.opponentCounterhit = defaultOpponentCounterhit; options.opponentFacing = defaultOpponentFacing;
    }
    else options = LastOptions();
    m_speed = options.speed;
    m_loop = options.loop;
    m_rootMotion = options.rootMotion;
    m_followCamera = options.follow;
    m_applyDisplacements = options.displacements;
    m_showHitboxes = options.hitboxes;
    m_showOpponent = options.opponent;
    m_opponentAirborne = options.opponentAirborne;
    m_opponentAirMove = options.opponentAirMove;
    m_opponentCounterhit = options.opponentCounterhit;
    m_opponentFacing = options.opponentFacing;
    m_opponentPushback = options.opponentPushback;
}

AnimationPreview::~AnimationPreview() {
    if (m_job.valid()) m_job.wait();
}

void AnimationPreview::WaitForDecode() {
    if (m_job.valid()) m_job.wait();
}

void AnimationPreview::Poll(const Input& input) {
    if (!m_loaded || input.token != m_token) {
        // A new animation: fetch its bytes here (editor data is not thread-safe), decode them on a worker thread.
        m_loaded = true; m_token = input.token;
        m_error.clear(); m_posedFrame = -1;
        if (m_job.valid()) m_job.wait();
        try {
            auto bytes = input.load ? input.load() : std::vector<Byte>{};
            if (bytes.empty()) throw std::runtime_error(_("edition.preview.no_animation"));
            m_job = std::async(std::launch::async, [bytes = std::move(bytes)] { return std::make_shared<AnimPreview::Clip>(DecodeClip(bytes)); });
        } catch (const std::exception& e) { m_clip.reset(); m_error = e.what(); }
    }
    if (synchronous && m_job.valid()) m_job.wait();
    if (m_job.valid() && m_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto clip = m_job.get();
            if (clip->frames.empty()) throw std::runtime_error(_("edition.preview.no_animation"));
            const bool first = !m_clip;
            m_clip = std::move(clip);
            if (first) m_frame = 0;
            m_frame = std::clamp(m_frame, 0.0f, float(m_clip->frames.size() - 1));
            m_posedFrame = -1;
        } catch (const std::exception& e) { m_clip.reset(); m_error = e.what(); }
    }
}

void AnimationPreview::RefreshDisplacements(const Input& input) {
    if (m_hitboxes != input.hitboxes) { m_hitboxes = input.hitboxes; m_posedFrame = -1; }
    if (!m_clip) return;
    if (m_displacedClip == m_clip.get() && m_displacedApplied == m_applyDisplacements && m_displacements == input.displacements) return;
    m_displacements = input.displacements;
    m_displacedApplied = m_applyDisplacements;
    m_displaced = m_applyDisplacements ? ApplyDisplacements(m_clip->frames, m_displacements) : m_clip->frames;
    m_displacedClip = m_clip.get();
    m_posedFrame = -1;
}

// Channels at a fractional frame: linear interpolation between the two keyframes around it (the last frame holds).
Channels AnimationPreview::ChannelsAt(float frame) const {
    const auto& frames = Frames();
    const int last = int(frames.size()) - 1;
    frame = std::clamp(frame, 0.0f, float(last));
    const int index = (std::min)(int(frame), last);
    const float t = frame - float(index);
    return t > 0 && index < last ? Interpolate(frames[index], frames[index + 1], t) : frames[index];
}

void AnimationPreview::UpdatePose(float frame) {
    if (!m_clip || m_clip->frames.empty() || (frame == m_posedFrame && m_posedRoot == m_rootMotion && m_posedHitboxes == m_showHitboxes)) return;
    const auto& model = Model::Embedded();
    const auto channels = ChannelsAt(frame);
    const auto pose = Pose(model, channels, {.rootMotion = m_rootMotion});
    Skin(model, pose, m_positions, m_normals);
    m_hitVertices.clear();
    if (m_showHitboxes && !m_hitboxes.empty()) {
        std::vector<Mat4> previous;
        if (frame >= 1) previous = Pose(model, ChannelsAt(frame - 1), {.rootMotion = m_rootMotion});
        // Drawn 30% thicker than computed, so that they read clearly over the body.
        for (auto shape : HitShapes(model, pose, previous, m_hitboxes, int(frame) + 1)) { shape.radius *= 1.3f; AppendCapsule(m_hitVertices, shape); }
    }
    m_posedHitboxes = m_showHitboxes;
    // Camera focus: the pelvis over the floor (smoothing would lag behind fast root motion).
    const int base = model.Find("BASE");
    if (base >= 0) m_shadow = {pose[base](0, 3), 0, pose[base](2, 3)};
    // The camera follows the root motion only (GlobalPos, turned like the body offset in Pose): body sway stays visible.
    m_focus = m_rootMotion ? Vec3{channels[2] / 1000, 0.95f, -channels[0] / 1000} : Vec3{0, 0.95f, 0};
    m_posedFrame = frame; m_posedRoot = m_rootMotion;
    if (m_gpu && m_gpu->ready) {
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu->positionVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_positions.size() * 4, m_positions.data());
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu->normalVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_normals.size() * 4, m_normals.data());
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu->hitVbo);
        glBufferData(GL_ARRAY_BUFFER, m_hitVertices.size() * 4, m_hitVertices.empty() ? nullptr : m_hitVertices.data(), GL_DYNAMIC_DRAW);
    }
}

const AnimPreview::Clip* AnimationPreview::OpponentClipFor(const Input::Animation& animation) {
    if (!animation.load) return nullptr;
    if (m_opponentClips.size() > 24 && !m_opponentClips.contains(animation.token)) m_opponentClips.clear(); // waits for their jobs
    auto& entry = m_opponentClips[animation.token];
    if (!entry.clip && !entry.job.valid()) {
        // Bytes fetched here (editor data is not thread-safe), decoded on a worker thread. A failure leaves an empty clip.
        try {
            auto bytes = animation.load();
            if (bytes.empty()) entry.clip = std::make_shared<AnimPreview::Clip>();
            else entry.job = std::async(std::launch::async, [bytes = std::move(bytes)] { return std::make_shared<AnimPreview::Clip>(DecodeClip(bytes)); });
        } catch (const std::exception&) { entry.clip = std::make_shared<AnimPreview::Clip>(); }
    }
    if (entry.job.valid() && (synchronous || entry.job.wait_for(std::chrono::seconds(0)) == std::future_status::ready)) {
        try { entry.clip = entry.job.get(); } catch (const std::exception&) { entry.clip = std::make_shared<AnimPreview::Clip>(); }
    }
    return entry.clip && !entry.clip->frames.empty() ? entry.clip.get() : nullptr;
}

void AnimationPreview::UpdateOpponent(const Input& input, float frame) {
    m_opponentVisible = false;
    if (!m_showOpponent || !m_clip || m_clip->frames.empty() || m_hitboxes.empty()) return;
    // Hit: the first hitbox frame (1-based).
    int hit = (std::numeric_limits<int>::max)();
    for (const auto& slot : m_hitboxes) hit = (std::min)(hit, slot.start);
    const float hitFrame = float((std::max)(hit, 1) - 1);
    const auto& opponent = input.opponent;
    if (!opponent.move) return;
    Input::Animation before = opponent.move(0x8001);
    if (m_opponentAirborne && m_opponentAirMove >= 0) {
        auto chosen = opponent.move(m_opponentAirMove);
        if (chosen.load) before = std::move(chosen);
    }
    const auto facing = static_cast<Facing>(m_opponentFacing);
    const int32_t reactionMove = (m_opponentAirborne ? opponent.airReactions : opponent.reactions).For(m_opponentCounterhit, facing);
    const Input::Animation reaction = reactionMove >= 0 ? opponent.move(reactionMove) : Input::Animation{};
    const bool hitting = frame >= hitFrame && reaction.load;
    const AnimPreview::Clip* beforeClip = OpponentClipFor(before);
    const AnimPreview::Clip* reactionClip = OpponentClipFor(reaction); // both decoded ahead, so the switch does not wait
    const AnimPreview::Clip* clip = hitting ? reactionClip : beforeClip;
    if (!clip) return;
    // Channels of a clip at a fractional frame. Before the hit the pose loops (idle); the reaction starts on the hit
    // frame and holds its last frame.
    auto channelsAt = [](const AnimPreview::Clip& c, float at, bool loop) {
        const float last = float(c.frames.size() - 1);
        if (loop) at = last > 0 ? std::fmod(at, last + 1) : 0;
        at = std::clamp(at, 0.0f, last);
        const int index = int(at);
        const float t = at - float(index);
        return t > 0 && index < int(last) ? Interpolate(c.frames[index], c.frames[index + 1], t) : c.frames[index];
    };
    const auto channels = hitting ? channelsAt(*clip, frame - hitFrame, false) : channelsAt(*clip, frame, true);

    // Target: in front of ours (+X), where the first hitbox reaches on the hit frame (our pose as shown), in line
    // with our pelvis.
    const auto& model = Model::Embedded();
    const int base = model.Find("BASE");
    const auto ours = Pose(model, ChannelsAt(hitFrame), {.rootMotion = m_rootMotion});
    std::vector<Mat4> previous;
    if (hitFrame >= 1) previous = Pose(model, ChannelsAt(hitFrame - 1), {.rootMotion = m_rootMotion});
    float targetX = -1e9f;
    for (const auto& shape : HitShapes(model, ours, previous, m_hitboxes, hit)) targetX = (std::max)({targetX, shape.a[0], shape.b[0]});
    const float oursX = base >= 0 ? ours[base](0, 3) : 0, oursZ = base >= 0 ? ours[base](2, 3) : 0;
    // The hitbox reaches into the body, not only its surface; no drawable hitbox (unknown limb): an arm's length ahead.
    targetX = targetX > -1e8f ? targetX + 0.15f : oursX + 0.8f;
    m_opponentDistance = targetX;

    // Posed like ours (facing +X, its left at -Z), turned about Y so that the hit side faces ours: front half a turn,
    // back none, left side a quarter turn one way, right side the other. Then moved so that its pelvis is on the
    // target when the hit lands (the reaction's first frame, else the pose it holds then): animations do not all
    // start over their origin.
    const float angle = facing == Facing::Front ? std::numbers::pi_v<float> : facing == Facing::Back ? 0.0f
        : facing == Facing::Left ? std::numbers::pi_v<float> / 2 : -std::numbers::pi_v<float> / 2;
    const float c = std::cos(angle), s = std::sin(angle);
    auto turn = [&](float& x, float& z, float offsetX, float offsetZ) { const float x0 = x, z0 = z; x = offsetX + x0 * c + z0 * s; z = offsetZ - x0 * s + z0 * c; };
    float anchorX = 0, anchorZ = 0;
    if (base >= 0) {
        const auto anchorPose = Pose(model, reactionClip ? channelsAt(*reactionClip, 0, false) : channelsAt(*beforeClip, hitFrame, true), {.rootMotion = m_rootMotion});
        anchorX = anchorPose[base](0, 3); anchorZ = anchorPose[base](2, 3);
        turn(anchorX, anchorZ, 0, 0);
    }
    float offsetX = targetX - anchorX;
    const float offsetZ = oursZ - anchorZ;
    // Pushback of the hit (the side's, as the reaction): away from ours (+X) from the hit frame on.
    if (hitting && m_opponentPushback) {
        const auto& reactions = m_opponentAirborne ? opponent.airReactions : opponent.reactions;
        const auto& pushback = reactions.PushbackFor(m_opponentCounterhit, facing);
        const float since = frame - hitFrame;
        offsetX += pushback.Distance(since) / 1000;
        // Airborne opponents are pushed further the more damage the hit does: 20 units per damage point, damage capped
        // at 50 (50 damage: 1000 units), spread over the pushback's frames (10 at least).
        if (m_opponentAirborne && reactions.damage > 0) {
            const float extra = 20.0f * float((std::min)(reactions.damage, 50));
            const float span = (std::max)(10.0f, float(pushback.offsets.size() + pushback.duration));
            offsetX += extra * (std::min)(1.0f, since / span) / 1000;
        }
    }
    const auto pose = Pose(model, channels, {.rootMotion = m_rootMotion});
    Skin(model, pose, m_opponentPositions, m_opponentNormals);
    for (size_t i = 0; i + 2 < m_opponentPositions.size(); i += 3) {
        turn(m_opponentPositions[i], m_opponentPositions[i + 2], offsetX, offsetZ);
        turn(m_opponentNormals[i], m_opponentNormals[i + 2], 0, 0);
    }
    if (base >= 0) { float x = pose[base](0, 3), z = pose[base](2, 3); turn(x, z, offsetX, offsetZ); m_opponentShadow = {x, 0, z}; }
    if (m_gpu && m_gpu->ready) {
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu->opponentPositionVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_opponentPositions.size() * 4, m_opponentPositions.data());
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu->opponentNormalVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_opponentNormals.size() * 4, m_opponentNormals.data());
    }
    m_opponentVisible = true;
}

void AnimationPreview::RenderViewport(float width, float height, const Input& input) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##previewViewport", ImVec2(width, height), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    // The wheel over the view zooms only: the view takes it, so the form around it does not scroll.
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    auto& io = ImGui::GetIO();
    // Left or middle drag orbits around the character, up to looking straight down on it.
    if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0))) {
        m_yaw -= io.MouseDelta.x * 0.01f;
        m_pitch = std::clamp(m_pitch + io.MouseDelta.y * 0.01f, -0.2f, 1.55f);
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
        m_panX -= io.MouseDelta.x * m_distance * 0.0015f;
        m_panY += io.MouseDelta.y * m_distance * 0.0015f;
    }
    if (hovered && io.MouseWheel != 0) m_distance = std::clamp(m_distance * std::pow(0.88f, io.MouseWheel), 0.8f, 14.0f);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { m_yaw = DefaultYaw; m_pitch = DefaultPitch; m_distance = DefaultDistance; m_panX = m_panY = 0; }
    if (hovered) ImGui::SetItemTooltip("%s", _("edition.preview.camera_help"));

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + width, origin.y + height);
    if (!GlAvailable() || (m_gpu && m_gpu->failed)) {
        draw->AddRectFilled(origin, end, IM_COL32(22, 24, 30, 255));
        const char* text = m_gpu && m_gpu->failed ? m_gpu->error.c_str() : _("edition.preview.no_renderer");
        draw->AddText(ImVec2(origin.x + 10, origin.y + 10), IM_COL32(200, 200, 200, 255), text);
        return;
    }
    const ImVec2 scale = io.DisplayFramebufferScale;
    RenderScene((std::max)(1, int(width * (scale.x > 0 ? scale.x : 1))), (std::max)(1, int(height * (scale.y > 0 ? scale.y : 1))), input);
    if (m_gpu && m_gpu->texture && !m_gpu->failed)
        draw->AddImage(ImTextureID(uintptr_t(m_gpu->texture)), origin, end, ImVec2(0, 1), ImVec2(1, 0));
    // Overlay: frame number and active state, top left.
    if (m_clip) {
        const int frame = int(m_frame) + 1;
        bool activeFrame = false;
        for (const auto& [first, last] : input.active) activeFrame |= frame >= first && frame <= last;
        const auto label = std::format("{} / {}", frame, m_clip->frames.size());
        draw->AddText(ImVec2(origin.x + 8, origin.y + 6), IM_COL32(230, 230, 235, 255), label.c_str());
        if (activeFrame) {
            const auto x = origin.x + 16 + ImGui::CalcTextSize(label.c_str()).x;
            draw->AddRectFilled(ImVec2(x - 4, origin.y + 5), ImVec2(x + ImGui::CalcTextSize(_("edition.preview.active")).x + 4, origin.y + 7 + ImGui::GetTextLineHeight()), IM_COL32(190, 40, 40, 230), 3);
            draw->AddText(ImVec2(x, origin.y + 6), IM_COL32(255, 255, 255, 255), _("edition.preview.active"));
        }
        draw->AddText(ImVec2(origin.x + 8, end.y - ImGui::GetTextLineHeight() - 6), IM_COL32(150, 155, 165, 255), m_clip->format.c_str());
    } else {
        const char* text = !m_error.empty() ? m_error.c_str() : _("edition.preview.loading");
        draw->AddText(ImVec2(origin.x + 10, origin.y + 10), m_error.empty() ? IM_COL32(200, 200, 200, 255) : IM_COL32(240, 120, 110, 255), text);
    }
    draw->AddRect(origin, end, IM_COL32(60, 66, 80, 255));
}

void AnimationPreview::Thumbnail(const Input& input, ImDrawList* draw, ImVec2 min, ImVec2 max, bool play) {
    Poll(input);
    // Thumbnails show the character alone, without hitboxes or displacement properties.
    m_showOpponent = false; m_showHitboxes = false;
    if (m_clip) {
        const int frames = int(Frames().size());
        if (play) {
            if (!m_thumbPlaying) m_frame = 0;
            m_frame += ImGui::GetIO().DeltaTime * 60;
            if (m_frame >= frames) m_frame = std::fmod(m_frame, float(frames));
        }
        else {
            // Still pose: the first active frame of an attack, the middle of the animation otherwise.
            const int still = !input.active.empty() ? input.active.front().first - 1 : frames / 2;
            m_frame = float(std::clamp(still, 0, frames - 1));
        }
    }
    m_thumbPlaying = play;
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    const int w = (std::max)(1, int((max.x - min.x) * (scale.x > 0 ? scale.x : 1))), h = (std::max)(1, int((max.y - min.y) * (scale.y > 0 ? scale.y : 1)));
    if (!GlAvailable() || (m_gpu && m_gpu->failed)) {
        draw->AddRectFilled(min, max, IM_COL32(22, 24, 30, 255));
        return;
    }
    // A still thumbnail is drawn once: the texture keeps it until the frame or the size changes.
    if (m_clip && (m_frame != m_thumbFrame || m_posedFrame != m_frame || w != m_thumbWidth || h != m_thumbHeight)) {
        RenderScene(w, h, input);
        m_thumbFrame = m_frame; m_thumbWidth = w; m_thumbHeight = h;
    }
    if (m_clip && m_gpu && m_gpu->texture && !m_gpu->failed)
        draw->AddImage(ImTextureID(uintptr_t(m_gpu->texture)), min, max, ImVec2(0, 1), ImVec2(1, 0));
    else {
        draw->AddRectFilled(min, max, IM_COL32(22, 24, 30, 255));
        const char* text = !m_error.empty() ? m_error.c_str() : _("edition.preview.loading");
        draw->PushClipRect(min, max, true);
        draw->AddText(ImVec2(min.x + 4, min.y + 4), m_error.empty() ? IM_COL32(200, 200, 200, 255) : IM_COL32(240, 120, 110, 255), text);
        draw->PopClipRect();
    }
}

void AnimationPreview::RenderScene(int w, int h, const Input& input) {
    try {
        if (!m_gpu) m_gpu = std::make_unique<Gpu>();
        GlState saved;
        if (!m_gpu->ready) m_gpu->Init();
        m_gpu->Resize(w, h);
        UpdatePose(m_frame);
        UpdateOpponent(input, m_frame);

        glBindFramebuffer(GL_FRAMEBUFFER, m_gpu->msaaFbo ? m_gpu->msaaFbo : m_gpu->resolveFbo);
        glViewport(0, 0, w, h);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
        if (m_gpu->msaaFbo) glEnable(GL_MULTISAMPLE);
        glClearColor(0.085f, 0.095f, 0.12f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Vec3 focus = m_followCamera ? m_focus : Vec3{0, 0.95f, 0};
        // With the opponent shown, the camera looks between the two (halfway to the opponent's place).
        if (m_opponentVisible) focus[0] = (focus[0] + m_opponentDistance) / 2;
        // Pan in the camera plane.
        const Vec3 forward{-std::cos(m_pitch) * std::sin(m_yaw), -std::sin(m_pitch), -std::cos(m_pitch) * std::cos(m_yaw)};
        const Vec3 right = Normalize(Cross(forward, {0, 1, 0})), up = Cross(right, forward);
        const Vec3 target{focus[0] + right[0] * m_panX + up[0] * m_panY, focus[1] + right[1] * m_panX + up[1] * m_panY, focus[2] + right[2] * m_panX + up[2] * m_panY};
        const Vec3 eye{target[0] - forward[0] * m_distance, target[1] - forward[1] * m_distance, target[2] - forward[2] * m_distance};
        const Mat4 viewProj = Perspective(FieldOfView(float(w) / float(h)), float(w) / float(h), 0.05f, 60.0f) * LookAt(eye, target, {0, 1, 0});

        glUseProgram(m_gpu->program);
        glUniformMatrix4fv(m_gpu->uViewProj, 1, GL_TRUE, viewProj.m.data());
        // Floor plane, fading out around the character.
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniform1i(m_gpu->uLit, 2); glUniform3f(m_gpu->uEye, focus[0], 0, focus[2]);
        glUniform4f(m_gpu->uColour, 0.16f, 0.18f, 0.22f, 1.0f);
        glBindVertexArray(m_gpu->planeVao); glDrawArrays(GL_TRIANGLES, 0, 6);
        glUniform4f(m_gpu->uColour, 0.30f, 0.33f, 0.40f, 0.9f);
        glBindVertexArray(m_gpu->linesVao); glDrawArrays(GL_LINES, 0, m_gpu->gridLines);
        glUniform4f(m_gpu->uColour, 0.45f, 0.50f, 0.62f, 1.0f);
        glDrawArrays(GL_LINES, m_gpu->gridLines, m_gpu->majorLines);
        // Shadow under the pelvis.
        auto shadow = [&](const Vec3& at) {
            std::vector<float> disc{at[0], 0.002f, at[2]};
            for (int i = 0; i <= 32; ++i) { const float a = i * 2 * std::numbers::pi_v<float> / 32; disc.insert(disc.end(), {at[0] + 0.40f * std::cos(a), 0.002f, at[2] + 0.34f * std::sin(a)}); }
            glBindBuffer(GL_ARRAY_BUFFER, m_gpu->discVbo); glBufferSubData(GL_ARRAY_BUFFER, 0, disc.size() * 4, disc.data());
            glUniform1i(m_gpu->uLit, 0); glUniform4f(m_gpu->uColour, 0, 0, 0, 0.35f);
            glBindVertexArray(m_gpu->discVao); glDrawArrays(GL_TRIANGLE_FAN, 0, m_gpu->discVertices);
        };
        if (m_clip) shadow(m_shadow);
        if (m_opponentVisible) shadow(m_opponentShadow);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        if (m_clip && !m_positions.empty()) {
            glUniform1i(m_gpu->uLit, 1); glUniform3f(m_gpu->uEye, eye[0], eye[1], eye[2]);
            glBindVertexArray(m_gpu->meshVao);
            for (const auto& material : Model::Embedded().materials) {
                glUniform4f(m_gpu->uColour, material.colour[0], material.colour[1], material.colour[2], 1);
                glDrawElements(GL_TRIANGLES, GLsizei(material.count * 3), GL_UNSIGNED_SHORT, reinterpret_cast<void*>(uintptr_t(material.first) * 6));
            }
        }
        // Opponent: the same mannequin in a teal scheme (the materials' brightness kept), so the two read apart.
        if (m_opponentVisible && !m_opponentPositions.empty()) {
            glUniform1i(m_gpu->uLit, 1); glUniform3f(m_gpu->uEye, eye[0], eye[1], eye[2]);
            glBindVertexArray(m_gpu->opponentVao);
            for (const auto& material : Model::Embedded().materials) {
                const float l = 0.3f * material.colour[0] + 0.59f * material.colour[1] + 0.11f * material.colour[2];
                glUniform4f(m_gpu->uColour, 0.06f + 0.40f * l, 0.16f + 0.80f * l, 0.18f + 0.72f * l, 1);
                glDrawElements(GL_TRIANGLES, GLsizei(material.count * 3), GL_UNSIGNED_SHORT, reinterpret_cast<void*>(uintptr_t(material.first) * 6));
            }
        }
        // Hitboxes: semi-transparent red over the body (depth tested, not written, so overlaps stay visible).
        if (m_clip && !m_hitVertices.empty()) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
            glUniform1i(m_gpu->uLit, 1); glUniform4f(m_gpu->uColour, 0.95f, 0.16f, 0.14f, 0.38f);
            glBindVertexArray(m_gpu->hitVao); glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_hitVertices.size() / 6));
            glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        }
        if (m_gpu->msaaFbo) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gpu->msaaFbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_gpu->resolveFbo);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    } catch (const std::exception& e) {
        if (!m_gpu) m_gpu = std::make_unique<Gpu>();
        m_gpu->failed = true; m_gpu->error = e.what();
    }
}

void AnimationPreview::RenderTimeline(const Input& input, int frames) {
    auto* draw = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x, height = 30;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##previewTimeline", ImVec2((std::max)(width, 1.0f), height));
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    const ImVec2 end(origin.x + width, origin.y + height);
    draw->AddRectFilled(origin, end, IM_COL32(28, 31, 38, 255), 3);
    if (frames <= 0) { draw->AddRect(origin, end, IM_COL32(60, 66, 80, 255), 3); return; }
    const float cell = width / frames;
    const float barTop = origin.y + 14, barBottom = end.y - 2;
    auto frameX = [&](int frame) { return origin.x + frame * cell; }; // left edge of 0-based frame
    // Move length past the animation's end, or animation frames past the move's end (dimmed).
    if (input.moveFrames > 0 && input.moveFrames < frames)
        draw->AddRectFilled(ImVec2(frameX(input.moveFrames), barTop), ImVec2(end.x, barBottom), IM_COL32(16, 17, 20, 255));
    // Active frames (1-based, inclusive).
    for (const auto& [first, last] : input.active) {
        if (last < first || last < 1 || first > frames) continue;
        const int a = (std::max)(first, 1) - 1, b = (std::min)(last, frames);
        draw->AddRectFilled(ImVec2(frameX(a), barTop), ImVec2(frameX(b), barBottom), IM_COL32(200, 44, 44, 255));
    }
    // Ticks: every frame when there is room, labels every 1/2/5/10/... frames.
    int step = 1;
    for (int s : {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000}) { step = s; if (s * cell >= 34) break; }
    for (int f = 0; f <= frames; ++f) {
        const float x = frameX(f);
        const bool label = f % step == 0;
        if (cell >= 4 || label) draw->AddLine(ImVec2(x, label ? barTop - 3 : barBottom - 5), ImVec2(x, barBottom), label ? IM_COL32(120, 126, 140, 255) : IM_COL32(70, 74, 86, 255));
        if (label && f < frames) {
            const auto text = std::to_string(f + 1);
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(x + 2, origin.y), IM_COL32(150, 155, 168, 255), text.c_str());
        }
    }
    // Frames that have displacement properties (cyan marks under the bar, dimmed when not applied).
    for (const auto& d : input.displacements) {
        if (d.frame < 1 || d.frame > frames) continue;
        const float x = frameX(d.frame - 1) + cell / 2;
        draw->AddTriangleFilled(ImVec2(x - 4, end.y), ImVec2(x + 4, end.y), ImVec2(x, end.y - 6), m_applyDisplacements ? IM_COL32(70, 200, 230, 255) : IM_COL32(70, 110, 130, 255));
    }
    // Playhead
    const int current = std::clamp(int(m_frame), 0, frames - 1);
    draw->AddRectFilled(ImVec2(frameX(current), barTop), ImVec2((std::max)(frameX(current + 1), frameX(current) + 2), barBottom), IM_COL32(255, 255, 255, 70));
    const float x = frameX(current) + cell / 2;
    draw->AddLine(ImVec2(x, origin.y + 10), ImVec2(x, end.y), IM_COL32(255, 220, 90, 255), 2);
    draw->AddTriangleFilled(ImVec2(x - 5, origin.y + 8), ImVec2(x + 5, origin.y + 8), ImVec2(x, origin.y + 14), IM_COL32(255, 220, 90, 255));
    draw->AddRect(origin, end, IM_COL32(60, 66, 80, 255), 3);
    const float mouse = ImGui::GetIO().MousePos.x;
    const int under = std::clamp(int((mouse - origin.x) / cell), 0, frames - 1);
    if (active) { m_frame = float(under); m_playing = false; }
    if (hovered) {
        bool inActive = false;
        for (const auto& [first, last] : input.active) inActive |= under + 1 >= first && under + 1 <= last;
        ImGui::SetTooltip("%s %d%s", _("edition.preview.frame"), under + 1, inActive ? _("edition.preview.active_suffix") : "");
    }
}

bool AnimationPreview::SideBySide() { return ImGui::GetContentRegionAvail().x >= SideBySideWidth; }

void AnimationPreview::SetOpen(bool open) {
    if (open == m_open) return;
    m_open = LastOpen() = open;
    Settings::Set(OpenSettingKey, open);
    if (!open) {
        // Not rendered at all while hidden: drop the framebuffers and buffers, they are rebuilt on reopening.
        m_viewportHovered = false;
        m_gpu.reset();
        m_posedFrame = -1;
    }
}

float AnimationPreview::ToggleButtonWidth() { return ImGui::GetFrameHeight(); }

bool AnimationPreview::ToggleButton() {
    ImGui::PushID(this);
    const float size = ToggleButtonWidth();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##previewToggle", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    if (clicked) SetOpen(!m_open);
    ImGui::SetItemTooltip("%s", _(m_open ? "edition.preview.hide" : "edition.preview.show"));
    // Icon: a screen with a play triangle; filled with the accent colour while the preview is shown.
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size, origin.y + size);
    const ImU32 background = ImGui::GetColorU32(m_open ? (held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive)
                                                      : (held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    draw->AddRectFilled(origin, end, background, ImGui::GetStyle().FrameRounding);
    const ImU32 ink = m_open ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 176, 190, 255);
    const float pad = size * 0.2f;
    const ImVec2 a(origin.x + pad, origin.y + pad * 1.15f), b(end.x - pad, end.y - pad * 1.15f);
    draw->AddRect(a, b, ink, 2, 0, 1.5f);
    const float cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2, t = (b.y - a.y) * 0.28f;
    draw->AddTriangleFilled(ImVec2(cx - t * 0.7f, cy - t), ImVec2(cx - t * 0.7f, cy + t), ImVec2(cx + t, cy), ink);
    ImGui::PopID();
    return clicked;
}

void AnimationPreview::LoopButton() {
    // Square icon button (frame height): a circular arrow, filled with the accent colour while looping.
    const float size = ImGui::GetFrameHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##loop", ImVec2(size, size))) m_loop = !m_loop;
    const bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImGui::SetItemTooltip("%s", _("edition.preview.loop"));
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size, origin.y + size);
    const ImU32 background = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : m_loop ? ImGuiCol_ButtonActive : ImGuiCol_Button);
    draw->AddRectFilled(origin, end, background, ImGui::GetStyle().FrameRounding);
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text, m_loop ? 1.0f : 0.55f);
    const float cx = origin.x + size / 2, cy = origin.y + size / 2, r = size * 0.27f;
    // Arc from the top going clockwise round to the left, with the arrow head at its end.
    constexpr float Start = -1.5708f, Sweep = 4.9f;
    draw->PathArcTo(ImVec2(cx, cy), r, Start, Start + Sweep, 20);
    draw->PathStroke(ink, 0, 1.6f);
    const float a = Start + Sweep, h = size * 0.16f;
    const ImVec2 tip(cx + std::cos(a) * r, cy + std::sin(a) * r);
    const ImVec2 tangent(-std::sin(a), std::cos(a)), normal(std::cos(a), std::sin(a));
    draw->AddTriangleFilled(ImVec2(tip.x + tangent.x * h, tip.y + tangent.y * h),
                            ImVec2(tip.x - tangent.x * h * 0.3f + normal.x * h * 0.8f, tip.y - tangent.y * h * 0.3f + normal.y * h * 0.8f),
                            ImVec2(tip.x - tangent.x * h * 0.3f - normal.x * h * 0.8f, tip.y - tangent.y * h * 0.3f - normal.y * h * 0.8f), ink);
}

void AnimationPreview::OpponentSettingsButton(const Input& input) {
    // Square icon button (frame height): a gear, opening the opponent's settings.
    const float size = ImGui::GetFrameHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##opponentSettings", ImVec2(size, size))) ImGui::OpenPopup("##opponentSettingsPopup");
    const bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImGui::SetItemTooltip("%s", _("edition.preview.opponent_settings"));
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size),
        ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), ImGui::GetStyle().FrameRounding);
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text, m_showOpponent ? 1.0f : 0.55f);
    const ImVec2 centre(origin.x + size / 2, origin.y + size / 2);
    const float r = size * 0.36f;
    for (int i = 0; i < 8; ++i) {
        const float a = i * std::numbers::pi_v<float> / 4;
        draw->AddLine(ImVec2(centre.x + std::cos(a) * r * 0.55f, centre.y + std::sin(a) * r * 0.55f),
                      ImVec2(centre.x + std::cos(a) * r, centre.y + std::sin(a) * r), ink, size * 0.13f);
    }
    draw->AddCircle(centre, r * 0.55f, ink, 16, size * 0.12f);

    if (!ImGui::BeginPopup("##opponentSettingsPopup")) return;
    ImGui::TextDisabled("%s", _("edition.preview.opponent_settings"));
    ImGui::Checkbox(_("edition.preview.opponent_airborne"), &m_opponentAirborne);
    if (m_opponentAirborne) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        ImGui::InputInt(_("edition.preview.opponent_air_move"), &m_opponentAirMove, 0, 0);
        m_opponentAirMove = (std::max)(m_opponentAirMove, -1);
        ImGui::SetItemTooltip("%s", _("edition.preview.opponent_air_move_help"));
        ImGui::Unindent();
    }
    ImGui::Checkbox(_("edition.preview.opponent_counterhit"), &m_opponentCounterhit);
    ImGui::SetItemTooltip("%s", _("edition.preview.opponent_counterhit_help"));
    ImGui::Checkbox(_("edition.preview.opponent_pushback"), &m_opponentPushback);
    ImGui::SetItemTooltip("%s", _("edition.preview.opponent_pushback_help"));
    ImGui::TextUnformatted(_("edition.preview.opponent_facing"));
    const char* sides[] = {"edition.preview.opponent_front", "edition.preview.opponent_back", "edition.preview.opponent_left", "edition.preview.opponent_right"};
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        ImGui::RadioButton(_(sides[i]), &m_opponentFacing, i);
    }
    ImGui::Separator();
    // What the opponent plays for these settings.
    const auto& reactions = m_opponentAirborne ? input.opponent.airReactions : input.opponent.reactions;
    const int32_t reaction = reactions.For(m_opponentCounterhit, static_cast<Facing>(m_opponentFacing));
    if (reaction >= 0) ImGui::Text(_("edition.preview.opponent_reaction"), reaction);
    else ImGui::TextDisabled("%s", _("edition.preview.opponent_no_reaction"));
    if (m_opponentAirborne && input.opponent.airReactions == input.opponent.reactions && reaction >= 0)
        ImGui::TextDisabled("%s", _("edition.preview.opponent_no_air_reaction"));
    ImGui::EndPopup();
}

void AnimationPreview::Render(const Input& input) {
    if (m_open) Body(input, 0);
}

void AnimationPreview::Body(const Input& input, float fillHeight) {
    ImGui::PushID(this);
    Poll(input);
    RefreshDisplacements(input);
    const int frames = m_clip ? int(m_clip->frames.size()) : 0;
    lastFrames = frames; lastFormat = m_clip ? m_clip->format : std::string(); lastError = m_error;
    if (forcedFrame >= 0 && frames > 0) { m_frame = float((std::min)(forcedFrame, frames - 1)); m_playing = false; }
    if (!std::isnan(forcedYaw)) m_yaw = forcedYaw;
    if (!std::isnan(forcedPitch)) m_pitch = forcedPitch;
    // Playback: game frames at 60 per second.
    if (m_clip && m_playing && frames > 0) {
        m_frame += ImGui::GetIO().DeltaTime * 60 * Speeds[m_speed];
        if (m_frame >= frames) {
            if (m_loop) m_frame = std::fmod(m_frame, float(frames));
            else { m_frame = float(frames - 1); m_playing = false; }
        }
    }
    const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
    // Below the view: controls and timeline, measured on the previous frame (they wrap in narrow panels).
    const float chrome = m_chromeHeight > 0 ? m_chromeHeight : ImGui::GetFrameHeightWithSpacing() + 30 + ImGui::GetStyle().ItemSpacing.y * 2;
    float height, maxHeight = 0;
    if (fillHeight > 0) height = (std::max)(MinHeight, fillHeight - chrome);
    else {
        // Above the fields: the preview (view, controls, timeline) takes at most half of the window's height,
        // unless that would leave the view under MinHeight. The splitter below the view resizes it.
        if (m_height <= 0) m_height = g_lastHeight;
        maxHeight = (std::max)(MinHeight, ImGui::GetWindowHeight() * 0.5f - chrome - 5);
        height = (std::min)(m_height, maxHeight);
    }
    RenderViewport(width, height, input);
    const bool viewportHovered = m_viewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const float chromeTop = ImGui::GetCursorPosY();
    if (fillHeight <= 0) {
        ImGui::InvisibleButton("##previewResize", ImVec2(width, 5));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) { m_height = std::clamp(height + ImGui::GetIO().MouseDelta.y, MinHeight, maxHeight); g_lastHeight = m_height; }
        ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetItemRectMin().x + width / 2 - 20, ImGui::GetItemRectMin().y + 2), ImVec2(ImGui::GetItemRectMin().x + width / 2 + 20, ImGui::GetItemRectMin().y + 2), IM_COL32(90, 96, 110, 255), 2);
    }
    else ImGui::Dummy(ImVec2(width, 1));

    // Controls, wrapped onto more lines when the panel is narrow.
    const auto& style = ImGui::GetStyle();
    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    auto next = [&](float itemWidth) {
        if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + itemWidth <= rowRight) ImGui::SameLine();
    };
    auto checkboxWidth = [&](const char* label) { return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x; };
    const bool hasClip = frames > 0;
    ImGui::BeginDisabled(!hasClip);
    auto step = [&](int delta) { m_playing = false; m_frame = float(std::clamp(int(m_frame) + delta, 0, (std::max)(frames - 1, 0))); };
    if (ImGui::ArrowButton("##previous", ImGuiDir_Left)) step(-1);
    ImGui::SetItemTooltip("%s", _("edition.preview.previous"));
    ImGui::SameLine(0, 2);
    if (ImGui::Button(m_playing ? _("edition.preview.pause") : _("edition.preview.play"), ImVec2(ImGui::CalcTextSize(_("edition.preview.pause")).x + ImGui::CalcTextSize(_("edition.preview.play")).x / 2 + 12, 0))) {
        if (!m_playing && int(m_frame) >= frames - 1) m_frame = 0;
        m_playing = !m_playing;
    }
    ImGui::SameLine(0, 2);
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) step(1);
    ImGui::SetItemTooltip("%s", _("edition.preview.next"));
    ImGui::SameLine(0, 2);
    LoopButton();
    next(70);
    ImGui::SetNextItemWidth(70);
    ImGui::Combo("##speed", &m_speed, SpeedLabels, IM_ARRAYSIZE(SpeedLabels));
    ImGui::SetItemTooltip("%s", _("edition.preview.speed"));
    next(checkboxWidth(_("edition.preview.root_motion")));
    ImGui::Checkbox(_("edition.preview.root_motion"), &m_rootMotion);
    ImGui::SetItemTooltip("%s", _("edition.preview.root_motion_help"));
    next(checkboxWidth(_("edition.preview.displacement")));
    // A move without displacement properties shows the box greyed out and unchecked; the remembered choice is kept for
    // the next move that has some.
    if (input.displacements.empty()) {
        bool none = false;
        ImGui::BeginDisabled();
        ImGui::Checkbox(_("edition.preview.displacement"), &none);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip()) {
            ImGui::TextUnformatted(_("edition.preview.displacement_disabled"));
            if (input.conditional > 0) ImGui::TextDisabled(_("edition.preview.displacement_conditional"), input.conditional);
            ImGui::EndTooltip();
        }
    }
    else ImGui::Checkbox(_("edition.preview.displacement"), &m_applyDisplacements);
    if (!input.displacements.empty() && ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(_("edition.preview.displacement_help"));
        ImGui::Separator();
        for (const auto& d : input.displacements) {
            const char* key = d.kind == Displacement::Kind::HorizontalScale ? "edition.preview.displacement_horizontal"
                : d.kind == Displacement::Kind::VerticalScale ? "edition.preview.displacement_vertical"
                : d.kind == Displacement::Kind::PushForward ? "edition.preview.displacement_forward" : "edition.preview.displacement_side";
            ImGui::Text("%s %d: 0x%X  %s %g", _("edition.preview.frame"), d.frame, d.id, _(key), d.value);
        }
        if (input.conditional > 0) ImGui::TextDisabled(_("edition.preview.displacement_conditional"), input.conditional);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    next(checkboxWidth(_("edition.preview.follow")));
    ImGui::Checkbox(_("edition.preview.follow"), &m_followCamera);
    ImGui::SetItemTooltip("%s", _("edition.preview.follow_help"));
    next(checkboxWidth(_("edition.preview.hitboxes")));
    ImGui::Checkbox(_("edition.preview.hitboxes"), &m_showHitboxes);
    ImGui::SetItemTooltip("%s", _("edition.preview.hitboxes_help"));
    next(checkboxWidth(_("edition.preview.opponent")) + style.ItemInnerSpacing.x + ImGui::GetFrameHeight());
    ImGui::Checkbox(_("edition.preview.opponent"), &m_showOpponent);
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(_("edition.preview.opponent_help"));
        if (input.hitboxes.empty()) ImGui::TextDisabled("%s", _("edition.preview.opponent_no_attack"));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    OpponentSettingsButton(input);
    ImGui::EndDisabled();
    if (!synchronous) RememberOptions({m_speed, m_loop, m_rootMotion, m_followCamera, m_applyDisplacements, m_showHitboxes,
                                       m_showOpponent, m_opponentAirborne, m_opponentCounterhit, m_opponentPushback, m_opponentAirMove, m_opponentFacing});
    if (hasClip) {
        next(ImGui::CalcTextSize("Frame 0000 / 0000").x);
        ImGui::TextDisabled("%s %d / %d", _("edition.preview.frame"), int(m_frame) + 1, frames);
    }
    RenderTimeline(input, frames);
    const bool timelineHovered = ImGui::IsItemHovered();
    // Keyboard while the pointer is over the preview: space plays/pauses, left/right step.
    if (hasClip && (viewportHovered || timelineHovered) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) m_playing = !m_playing;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) step(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) step(1);
    }
    ImGui::Spacing();
    m_chromeHeight = ImGui::GetCursorPosY() - chromeTop;
    ImGui::PopID();
}
