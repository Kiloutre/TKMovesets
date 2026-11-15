#pragma once
#include "AnimationPreview/PreviewClip.hpp"
#include "AnimationPreview/PreviewDisplacement.hpp"
#include "AnimationPreview/PreviewHitbox.hpp"
#include <functional>
#include <limits>
#include <map>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Animation preview of a move, drawn inside its editor form: the mannequin posed by the move's body animation
// (T7 0xC8 / 0x64 or T8 PANM, AnimPreview::DecodeClip), rendered with OpenGL into a texture, and a timeline to play,
// pause and step through the frames. The move's active frames are shown in red on the timeline.
class AnimationPreview {
public:
    // Inclusive range of 1-based move frames (frame 1 is the animation's first frame).
    using FrameRange = std::pair<int, int>;
    struct Input {
        // Changes when the move's animation changes; the animation is then fetched again with `load`.
        uint64_t token = 0;
        // Returns the animation bytes (called on the UI thread). May throw: the message is shown in place of the preview.
        std::function<std::vector<Byte>()> load;
        std::vector<FrameRange> active;
        // Frames the move lasts (0: the animation's length).
        int moveFrames = 0;
        // The move's known displacement properties (AnimPreview::ApplyDisplacements), applied when the "Displacement"
        // box is ticked; `conditional`: matching properties left out because they have conditions.
        std::vector<AnimPreview::Displacement> displacements;
        int conditional = 0;
        // Hitbox slots (AnimPreview::HitShapes), drawn as semi-transparent red shapes on their frames when the
        // "Hitboxes" box is ticked.
        std::vector<AnimPreview::HitboxSlot> hitboxes;
        // Opponent mannequin ("Opponent" box, off by default): a second character on our attacks (moves with hitboxes),
        // turned to the side the hit lands on and placed so the first hitbox reaches it. Until the first hitbox frame it
        // plays the idle move (0x8001), or when airborne the move chosen in its settings; then the reaction for its state:
        // the hit condition matching it (airborne or grounded), then ReactionMoves::For.
        struct Animation {
            uint64_t token = 0;
            std::function<std::vector<Byte>()> load; // empty: no such move
        };
        struct Opponent {
            AnimPreview::ReactionMoves reactions, airReactions; // grounded / airborne opponent
            // Animation of a move id (aliases allowed).
            std::function<Animation(int moveId)> move;
        } opponent;
    };
    AnimationPreview();
    AnimationPreview(const AnimationPreview&) = delete;
    AnimationPreview& operator=(const AnimationPreview&) = delete;
    ~AnimationPreview();
    // The preview is shown or hidden with ToggleButton (an icon the forms put next to their "Animation" button). Hidden,
    // nothing is decoded or rendered (and the GPU objects are released). Each form keeps its own state; a new form
    // starts in the state last chosen in any form, which is saved in the settings.
    // Stacked above the fields: at most half of the window's height, with a splitter to resize it. Nothing when hidden.
    void Render(const Input& input);
    // fillHeight > 0: fills that height (view, controls and timeline), for forms that put the preview beside the
    // fields; 0: as Render.
    void Body(const Input& input, float fillHeight);
    // Square icon button (frame height) that shows / hides the preview; returns true when clicked.
    bool ToggleButton();
    static float ToggleButtonWidth();
    bool IsOpen() const { return m_open; }
    // Whether the pointer was over the 3D view when it was last drawn.
    bool ViewportHovered() const { return m_viewportHovered; }
    // Whether the current content region is wide enough to put the preview beside the fields.
    static bool SideBySide();
    // Decoded clip, for tests (nullptr until the background decode finished).
    const AnimPreview::Clip* Clip() const { return m_clip.get(); }
    int Frame() const { return int(m_frame); }
    void SetFrame(int frame) { m_frame = float(frame); m_playing = false; }
    // Blocks until the pending decode (if any) finished, for tests and offscreen captures.
    void WaitForDecode();
    // Offscreen captures (anim-preview-shot): every preview decodes synchronously and shows this frame (0-based) when >= 0.
    static inline bool synchronous = false;
    static inline int forcedFrame = -1;
    // Camera override for captures: yaw (radians) when not NaN.
    static inline float forcedYaw = std::numeric_limits<float>::quiet_NaN();
    static inline float forcedPitch = std::numeric_limits<float>::quiet_NaN();
    // Initial state of the "Displacement" box of new previews in captures (synchronous). Outside captures, new previews
    // start with the speed and boxes last chosen in any preview, which are saved in the settings.
    static inline bool defaultDisplacements = true;
    // Opponent options of new previews in captures: shown, airborne, counter hit, facing (AnimPreview::Facing).
    static inline bool defaultOpponent = false, defaultOpponentAirborne = false, defaultOpponentCounterhit = false;
    static inline int defaultOpponentFacing = 0;
    // Frame count and format of the clip the last rendered preview shows (0 / empty: none), for captures.
    static inline int lastFrames = 0;
    static inline std::string lastFormat;
    static inline std::string lastError;

private:
    struct Gpu;
    std::unique_ptr<Gpu> m_gpu;
    uint64_t m_token = 0;
    bool m_loaded = false;
    std::future<std::shared_ptr<AnimPreview::Clip>> m_job;
    std::shared_ptr<AnimPreview::Clip> m_clip;
    std::string m_error;
    // Playback
    float m_frame = 0;
    bool m_playing = true, m_loop = true, m_rootMotion = true, m_followCamera = true;
    int m_speed = 4; // index in the speed table (1x)
    // Camera (orbit around the character)
    // Default view: from the side, a little in front, the character facing right (as player 1 in the game).
    static constexpr float DefaultYaw = 0.45f, DefaultPitch = 0.16f, DefaultDistance = 3.6f;
    float m_yaw = DefaultYaw, m_pitch = DefaultPitch, m_distance = DefaultDistance;
    float m_panX = 0, m_panY = 0;
    float m_height = 0; // viewport height, 0 = default
    bool m_open = true;
    bool m_viewportHovered = false;
    float m_chromeHeight = 0; // controls and timeline, as last drawn
    float m_posedFrame = -1; // fractional: playback interpolates between keyframes
    bool m_posedRoot = true;
    // Displacement properties: the clip's frames with them applied, rebuilt when the clip or the properties change.
    bool m_applyDisplacements = defaultDisplacements;
    std::vector<AnimPreview::Displacement> m_displacements;
    std::vector<AnimPreview::Channels> m_displaced;
    const AnimPreview::Clip* m_displacedClip = nullptr;
    bool m_displacedApplied = false;
    // Hitboxes: the slots, and the shapes' triangles (position + normal) for the posed frame.
    bool m_showHitboxes = true;
    std::vector<AnimPreview::HitboxSlot> m_hitboxes;
    std::vector<float> m_hitVertices;
    bool m_posedHitboxes = true;
    // Opponent: options, the decoded clips of its animations (by token, decoded on worker threads), its skinned mesh.
    bool m_showOpponent = false, m_opponentAirborne = false, m_opponentCounterhit = false, m_opponentPushback = true;
    int m_opponentAirMove = -1;
    int m_opponentFacing = 0; // AnimPreview::Facing
    float m_opponentDistance = 0;
    struct OpponentClip { std::future<std::shared_ptr<AnimPreview::Clip>> job; std::shared_ptr<AnimPreview::Clip> clip; };
    std::map<uint64_t, OpponentClip> m_opponentClips;
    std::vector<float> m_opponentPositions, m_opponentNormals;
    std::array<float, 3> m_opponentShadow{};
    bool m_opponentVisible = false;
    const AnimPreview::Clip* OpponentClipFor(const Input::Animation& animation);
    void UpdateOpponent(const Input& input, float frame);
    void OpponentSettingsButton(const Input& input);
    const std::vector<AnimPreview::Channels>& Frames() const { return m_displacedClip == m_clip.get() && m_clip ? m_displaced : m_clip->frames; }
    void RefreshDisplacements(const Input& input);
    void SetOpen(bool open);
    std::vector<float> m_positions, m_normals;
    std::array<float, 3> m_focus{0, 0.95f, 0}, m_shadow{};

    void Poll(const Input& input);
    void RenderViewport(float width, float height, const Input& input);
    void RenderTimeline(const Input& input, int frames);
    void LoopButton();
    // Poses the mannequin at a fractional frame, interpolating linearly between the keyframes around it.
    void UpdatePose(float frame);
    AnimPreview::Channels ChannelsAt(float frame) const;
};
