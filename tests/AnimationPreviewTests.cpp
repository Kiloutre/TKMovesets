// Animation preview checks, run through the exe's headless mode (TKMovesets2/main/headless.cpp):
//   anim-preview-check REFERENCE.json       pose and skinning against Blender (tools/build_preview_mannequin.py)
//   anim-preview-shot FILE MOVE OUT_DIR [FRAMES] [yaw=RADIANS] [pitch=RADIANS] [preview=off] [displacement=off] [opponent=SIDE[+air][+ch]] [size=WxH]
//       opens FILE in its editor inside a hidden GLFW window with the GUI's OpenGL backend, opens the move form and
//       writes OUT_DIR/frame_NNNN.png for each frame (FRAMES: "all", "A-B" or a comma list of 1-based frames).
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <zlib.h>

#include "AnimationPreview.hpp"
#include "AnimationPreview/PreviewModel.hpp"
#include "AppTheme.hpp"
#include "EditorForm.hpp"
#include "EditorVisuals.hpp"
#include "EditorVisuals_t7/EditorVisuals_t7.hpp"
#include "EditorVisuals_t8Retail/EditorVisuals_t8Retail.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "FileIO.hpp"
#include "GameAddressesFile.hpp"
#include "Games.hpp"
#include "LocalStorage.hpp"
#include "Localization.hpp"
#include "MovesetFile.hpp"
#include "Settings.hpp"
#include "TEditor.hpp"
#include "helpers.hpp"
#include "Editor_t7.hpp"
#include "GameData.hpp"
#include "GameProcess.hpp"
#include <map>
#include <set>
#include "Animations.hpp"
#include "RetailPanm.hpp"
#include "Structs_t7.h"
#include "T7AnimToRetailT8.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

void WritePng(const std::filesystem::path& path, int width, int height, const std::vector<unsigned char>& rgba) {
    // Rows bottom-up (glReadPixels) -> top-down, filter 0, RGB.
    std::vector<unsigned char> raw;
    raw.reserve(size_t(height) * (width * 3 + 1));
    for (int y = height - 1; y >= 0; --y) {
        raw.push_back(0);
        for (int x = 0; x < width; ++x) { const auto* p = &rgba[(size_t(y) * width + x) * 4]; raw.insert(raw.end(), {p[0], p[1], p[2]}); }
    }
    uLongf packedSize = compressBound(uLong(raw.size()));
    std::vector<unsigned char> packed(packedSize);
    Require(compress2(packed.data(), &packedSize, raw.data(), uLong(raw.size()), 6) == Z_OK, "PNG compression failed");
    packed.resize(packedSize);
    std::vector<unsigned char> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    auto chunk = [&](const char* type, const std::vector<unsigned char>& data) {
        auto be = [&](uint32_t v) { png.insert(png.end(), {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}); };
        be(uint32_t(data.size()));
        const size_t start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        be(uint32_t(crc32(0, png.data() + start, uInt(png.size() - start))));
    };
    std::vector<unsigned char> header(13);
    auto put = [&](size_t at, uint32_t v) { header[at] = uint8_t(v >> 24); header[at + 1] = uint8_t(v >> 16); header[at + 2] = uint8_t(v >> 8); header[at + 3] = uint8_t(v); };
    put(0, width); put(4, height); header[8] = 8; header[9] = 2;
    chunk("IHDR", header); chunk("IDAT", packed); chunk("IEND", {});
    Require(FileIO::WriteAtomically(path, std::span<const Byte>(png.data(), png.size())), "Cannot write " + path.string());
}
}

int CheckAnimationPreview(const std::wstring& referencePath) {
    using namespace AnimPreview;
    std::ifstream file{std::filesystem::path(referencePath)};
    Require(file.good(), "Cannot read the reference");
    const auto reference = nlohmann::json::parse(file);
    const auto& model = Model::Embedded();
    // Displacement properties on a synthetic clip: root +10 forward, +4 left and +5 height per frame.
    {
        std::vector<Channels> clip(6, Channels{});
        for (size_t f = 0; f < clip.size(); ++f) { clip[f][2] = 10.0f * f; clip[f][0] = 4.0f * f; clip[f][4] = 1000 + 5.0f * f; }
        using K = Displacement::Kind;
        const std::vector<Displacement> list = {{3, K::HorizontalScale, 50, 0x81E1}, {4, K::VerticalScale, 0, 0x81E2},
            {2, K::PushForward, -100, 0x8206}, {5, K::PushSide, 30, 0x8205}};
        const auto out = ApplyDisplacements(clip, list);
        // Samples are frames 1-6. The motion into frames 3+ is halved; the forward push applies from frame 2, the side
        // push from frame 5; the height stops changing from frame 4.
        const float forward[] = {0, 10 - 100, 15 - 100, 20 - 100, 25 - 100, 30 - 100}, left[] = {0, 4, 6, 8, 10 + 30, 12 + 30},
                    height[] = {1000, 1005, 1010, 1010, 1010, 1010};
        for (size_t f = 0; f < out.size(); ++f)
            Require(std::fabs(out[f][2] - forward[f]) < 1e-4f && std::fabs(out[f][0] - left[f]) < 1e-4f && std::fabs(out[f][4] - height[f]) < 1e-4f,
                std::format("ApplyDisplacements frame {}: {} {} {}", f, out[f][2], out[f][0], out[f][4]));
        Require(DisplacementFrame(0x8001) == 1 && DisplacementFrame(0) == 1 && DisplacementFrame(12) == 12, "DisplacementFrame");
        Require(DisplacementValue(K::PushForward, uint32_t(-4500), false) == -4500 && DisplacementValue(K::PushForward, 0xFFFFFC18u, true) == -1000,
            "DisplacementValue");
    }
    double boneError = 0, vertexError = 0;
    std::string worstBone;
    size_t vertices = 0;
    for (const auto& frame : reference.at("frames")) {
        Channels channels{};
        const auto& values = frame.at("values");
        Require(values.size() == channels.size(), "Reference frame is not 69 floats");
        for (size_t i = 0; i < channels.size(); ++i) channels[i] = values[i].get<float>();
        // The reference uses applyRotationFromAnimdata's own translation, (x0 + x1, y1, z1 - z0); the preview turns the
        // game axes instead (x = z0 + z1, z = -(x0 + x1)). Same body offset through other channel values:
        const float blenderX = channels[0] + channels[3], blenderZ = channels[5] - channels[2];
        channels[0] = channels[2] = 0;
        channels[5] = blenderX;
        channels[3] = -blenderZ;
        const auto pose = Pose(model, channels);
        for (const auto& [name, matrix] : frame.at("bones").items()) {
            const int bone = model.Find(name);
            Require(bone >= 0, "Model lacks bone " + name);
            for (int i = 0; i < 16; ++i) {
                const double e = std::fabs(pose[bone].m[i] - matrix[i].get<double>());
                if (e > boneError) { boneError = e; worstBone = name; }
            }
        }
        std::vector<float> positions, normals;
        Skin(model, pose, positions, normals);
        for (const auto& v : frame.at("vertices")) {
            const size_t index = v[0].get<size_t>();
            Require(index < model.vertices.size(), "Reference vertex out of range");
            for (int c = 0; c < 3; ++c) vertexError = (std::max)(vertexError, std::fabs(double(positions[index * 3 + c]) - v[1 + c].get<double>()));
            ++vertices;
        }
    }
    // 8-bit skin weights against Blender's float weights: allow 2 mm.
    const bool ok = boneError < 1e-3 && vertexError < 2e-3;
    std::cout << nlohmann::json({{"ok", ok}, {"frames", reference.at("frames").size()}, {"vertices", vertices}, {"max_bone_error", boneError},
        {"worst_bone", worstBone}, {"max_vertex_error_m", vertexError}, {"model_bones", model.bones.size()}, {"model_vertices", model.vertices.size()}}).dump() << '\n';
    return ok ? 0 : 1;
}

// anim-preview-joints ANIMATION OUT.json: armature-space positions of the main mannequin joints per frame, for
// comparisons against other skeletons (e.g. the native T8 PANM skeleton of the same animation).
int DumpAnimationPreviewJoints(const std::wstring& animationPath, const std::wstring& outputPath) {
    using namespace AnimPreview;
    std::ifstream in{std::filesystem::path(animationPath), std::ios::binary};
    Require(in.good(), "Cannot read the animation");
    std::vector<Byte> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto clip = DecodeClip(bytes);
    const auto& model = Model::Embedded();
    // Every bone of the model, and the raw root channels (GlobalPos and LocalPos, game units).
    nlohmann::json frames = nlohmann::json::array();
    for (const auto& channels : clip.frames) {
        const auto pose = Pose(model, channels);
        nlohmann::json joints;
        for (size_t b = 0; b < model.bones.size(); ++b) joints[model.bones[b].name] = {pose[b](0, 3), pose[b](1, 3), pose[b](2, 3)};
        joints["_global_pos"] = {channels[0], channels[1], channels[2]};
        joints["_local_pos"] = {channels[3], channels[4], channels[5]};
        frames.push_back(std::move(joints));
    }
    Require(FileIO::WriteAtomically(outputPath, nlohmann::json({{"format", clip.format}, {"frames", frames}}).dump()), "Cannot write the joints");
    std::cout << nlohmann::json({{"ok", true}, {"frames", clip.frames.size()}, {"format", clip.format}}).dump() << '\n';
    return 0;
}

// anim-preview-export T7_FILE MOVE OUT [panm]: a T7 move's animation bytes, or with "panm" its conversion onto the
// built-in retail skeleton (T7AnimToRetailT8::Convert, the converter's path), for the skeleton comparisons.
int ExportAnimationPreviewSource(std::span<const std::wstring> arguments) {
    Require(arguments.size() == 3 || (arguments.size() == 4 && arguments[3] == L"panm"), "anim-preview-export takes T7_FILE MOVE OUT [panm]");
    MovesetFile file(arguments[0]); file.Validate();
    Require(file.Header().gameId == GameId_T7, "Input must be a T7 moveset");
    StructsT7::TKMovesetHeaderBlocks blocks;
    memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
    const auto payload = file.Payload();
    const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
    const auto row = std::stoull(arguments[1], nullptr, 0);
    Require(row < info.table.moveCount, "Move row outside the moveset");
    const auto& move = *reinterpret_cast<const StructsT7_gameAddr::Move*>(payload.data() + blocks.movesetBlock + info.table.move + row * sizeof(StructsT7_gameAddr::Move));
    const auto anims = payload.subspan(blocks.animationBlock, blocks.motaBlock - blocks.animationBlock);
    Require(uint64_t(move.anim_addr) < anims.size(), "Move animation outside the animation block");
    const auto source = anims.subspan(size_t(move.anim_addr));
    const auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(source.data(), source.size());
    Require(size > 0 && size <= source.size(), "Cannot size the animation");
    std::vector<Byte> bytes(source.begin(), source.begin() + size);
    if (arguments.size() == 4) bytes = T7AnimToRetailT8::Convert(RetailPanm::DecodeT7(bytes));
    Require(FileIO::WriteAtomically(arguments[2], bytes), "Cannot write the output");
    std::cout << nlohmann::json({{"ok", true}, {"bytes", bytes.size()}}).dump() << '\n';
    return 0;
}

// anim-preview-scan FILE: decodes and poses every move animation of a T7 or retail T8 moveset as the move forms do
// (EditorT7::MoveAnimation / EditorT8Retail::MoveAnimation, then DecodeClip, Pose and Skin of every frame).
int ScanAnimationPreview(const std::wstring& path) {
    using namespace AnimPreview;
    MovesetFile file(path); file.Validate();
    const auto* game = Games::GetGameInfoFromIdentifier(file.Header().gameId, file.Header().minorVersion);
    Require(game && game->editorLogic, "This game has no editor");
    GameProcess process; GameData data; GameAddressesFile addresses;
    data.SetProcess(&process, &addresses); data.SetCurrentGame(game);
    std::unique_ptr<EditorLogic> editor(Games::FactoryGetEditorLogic(game, process, data));
    // As EditorVisuals: the file as stored (LoadMoveset decompresses it).
    uint64_t size = 0;
    Byte* stored = Helpers::ReadMovesetFile(path, size);
    Require(editor->LoadMoveset(stored, size), "The editor rejected the file");
    auto* t7 = dynamic_cast<EditorT7*>(editor.get());
    auto* t8 = dynamic_cast<EditorT8Retail*>(editor.get());
    Require(t7 || t8, "Only T7 and retail T8 editors have the preview");
    const uint32_t moves = t8 ? uint32_t(t8->Count(12)) : uint32_t(t7->MoveCount());
    std::map<std::string, size_t> errors, formats;
    std::set<uint64_t> seen;
    size_t decoded = 0, failed = 0, frames = 0;
    const auto& model = Model::Embedded();
    std::vector<float> positions, normals;
    // Displacement properties: per property id, the moves using it (a few examples) and the parameters seen.
    std::map<uint32_t, size_t> displacementMoves; std::map<uint32_t, std::vector<std::string>> displacementExamples;
    size_t conditionalTotal = 0;
    for (uint32_t move = 0; move < moves; ++move) {
        int conditional = 0;
        const auto list = t8 ? t8->MoveDisplacements(move, &conditional) : t7->MoveDisplacements(uint16_t(move));
        conditionalTotal += conditional;
        std::set<uint32_t> ids;
        for (const auto& d : list) {
            ids.insert(d.id);
            auto& examples = displacementExamples[d.id];
            if (examples.size() < 6) examples.push_back(std::format("move {} frame {} value {}", move, d.frame, d.value));
        }
        for (uint32_t id : ids) ++displacementMoves[id];
    }
    // Hitbox locations in use: T7 move hitbox_location, T8 hitbox slot locations (nonzero).
    std::map<std::string, size_t> hitboxLocations;
    if (t7) for (uint32_t move = 0; move < moves; ++move) { const uint32_t h = t7->MoveHitboxLocation(uint16_t(move)); if (h) ++hitboxLocations[std::format("0x{:X}", h)]; }
    if (t8) for (uint32_t move = 0; move < moves; ++move) for (const auto& slot : t8->MoveHitboxes(move)) { ++hitboxLocations[std::format("0x{:X}", slot.location)]; ++hitboxLocations[std::format("radius {}", slot.radius)]; ++hitboxLocations[std::format("height {}", slot.verticalHeight)]; }
    nlohmann::json displacementJson = nlohmann::json::object();
    for (const auto& [id, count] : displacementMoves) displacementJson[std::format("0x{:X}", id)] = {{"moves", count}, {"examples", displacementExamples[id]}};
    for (uint32_t move = 0; move < moves; ++move) {
        try {
            const uint64_t token = t8 ? move : t7->MoveAnimationToken(uint16_t(move));
            if (t7 && !seen.insert(token).second) continue;
            const auto bytes = t8 ? t8->MoveAnimation(move) : t7->MoveAnimation(uint16_t(move));
            const auto clip = DecodeClip(bytes);
            for (const auto& channels : clip.frames) {
                Skin(model, Pose(model, channels), positions, normals);
                for (float v : positions) Require(std::isfinite(v), "Non-finite vertex");
            }
            ++decoded; frames += clip.frames.size(); ++formats[clip.format];
        } catch (const std::exception& e) { ++failed; ++errors[e.what()]; }
    }
    nlohmann::json errorList = nlohmann::json::object();
    for (const auto& [message, count] : errors) errorList[message] = count;
    std::cout << nlohmann::json({{"ok", decoded > 0}, {"moves", moves}, {"decoded", decoded}, {"failed", failed}, {"frames", frames},
        {"formats", formats}, {"errors", errorList}, {"displacements", displacementJson}, {"conditional_displacements", conditionalTotal}, {"hitbox_locations", hitboxLocations}}).dump() << '\n';
    return decoded > 0 ? 0 : 1;
}

int ShootAnimationPreview(std::span<const std::wstring> arguments) {
    // arguments: FILE MOVE OUT_DIR [FRAMES] [yaw=R] [pitch=R] [preview=off] [displacement=off] [opponent=SIDE[+air][+ch]] [size=WxH]
    Require(arguments.size() >= 3, "anim-preview-shot takes FILE MOVE OUT_DIR [FRAMES] [yaw=RADIANS] [pitch=RADIANS] [preview=off] [displacement=off] [opponent=SIDE[+air][+ch]] [size=WxH]");
    const std::filesystem::path input = std::filesystem::absolute(arguments[0]), output = arguments[2];
    const auto moveText = Helpers::to_utf8(arguments[1]);
    std::string frames = "1";
    int width = 1600, height = 1000;
    bool previewOff = false;
    for (size_t i = 3; i < arguments.size(); ++i) {
        const auto a = Helpers::to_utf8(arguments[i]);
        if (a.starts_with("yaw=")) AnimationPreview::forcedYaw = std::stof(a.substr(4));
        else if (a.starts_with("pitch=")) AnimationPreview::forcedPitch = std::stof(a.substr(6));
        else if (a == "displacement=off") AnimationPreview::defaultDisplacements = false;
        else if (a.starts_with("opponent=")) {
            // opponent=front|back|left|right, optionally +air and +ch: the opponent mannequin shown in that state.
            AnimationPreview::defaultOpponent = true;
            const std::string spec = "+" + a.substr(9) + "+";
            const char* sides[] = {"+front+", "+back+", "+left+", "+right+"};
            for (int i = 0; i < 4; ++i) if (spec.find(sides[i]) != std::string::npos) AnimationPreview::defaultOpponentFacing = i;
            AnimationPreview::defaultOpponentAirborne = spec.find("+air+") != std::string::npos;
            AnimationPreview::defaultOpponentCounterhit = spec.find("+ch+") != std::string::npos;
        }
        else if (a == "preview=off") {
            previewOff = true;
            // Forms open with the preview collapsed (the saved state), in a settings file of the capture's own.
            Settings::SetFilePath(std::filesystem::temp_directory_path() / "tkm-anim-preview-shot.ini");
            Settings::Set("editor.animation_preview_open", false);
        }
        else if (a.starts_with("open=")) EditorForm::forceOpenCategories.insert(a.substr(5));  // a form section, e.g. open=Hitbox 1
        else if (a.starts_with("size=")) { Require(sscanf_s(a.c_str() + 5, "%dx%d", &width, &height) == 2, "size=WxH"); }
        else frames = a;
    }
    std::filesystem::create_directories(output);

    MovesetFile file(input.wstring());
    file.Validate();
    movesetInfo info{};
    info.filename = input.wstring();
    info.name = input.stem().string();
    info.gameId = file.Header().gameId;
    info.minorVersion = file.Header().minorVersion;
    const auto* game = Games::GetGameInfoFromIdentifier(info.gameId, info.minorVersion);
    Require(game && game->editorVisuals, "This game has no editor");

    Require(glfwInit(), "glfwInit failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(width, height, "anim-preview-shot", nullptr, nullptr);
    Require(window != nullptr, "Cannot create the hidden window");
    glfwMakeContextCurrent(window);
    Require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "gladLoadGL failed");
    Localization::LoadFile("en-US");
    ImGuiContext* context = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    AppTheme::Apply();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 130");
    AnimationPreview::synchronous = true;

    int written = 0;
    std::string clipFormat;
    int clipFrames = 0;
    {
        GameAddressesFile addresses;
        std::unique_ptr<EditorVisuals> view(Games::FactoryGetEditorVisuals(game, &info, &addresses, nullptr));
        Require(view && view->popen, "The editor could not open the file");
        auto* retail = dynamic_cast<EditorVisualsT8Retail*>(view.get());
        uint32_t move = uint32_t(std::stoul(moveText, nullptr, 0));
        if (retail) move = retail->Editor()->ResolveMove(move);
        auto frame = [&](bool capture, const std::string& name) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            io.DisplaySize = ImVec2(float(width), float(height));
            io.DeltaTime = 1.0f / 60;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            view->Render(700);
            // The editor sizes its window to 1280x720 when it first appears: stretch it to the capture size.
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if (!(w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) && !w->DockIsActive && w->Active)
                    { ImGui::SetWindowPos(w->Name, ImVec2(0, 0)); ImGui::SetWindowSize(w->Name, io.DisplaySize); }
            ImGui::Render();
            glViewport(0, 0, width, height);
            glClearColor(0.06f, 0.06f, 0.06f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if (capture) {
                std::vector<unsigned char> pixels(size_t(width) * height * 4);
                glReadBuffer(GL_BACK);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                WritePng(output / name, width, height, pixels);
                ++written;
            }
            glfwSwapBuffers(window);
        };
        frame(false, "");
        // T7: the move list shows the move too (highlighted, scrolled to it).
        if (retail) Require(retail->OpenRecord(12, move), "Cannot open the move record");
        else {
            Require(view->OpenFormWindow(TEditorWindowType_Move, uint16_t(move)), "Cannot open the move form");
            if (auto* t7 = dynamic_cast<EditorVisuals_T7*>(view.get())) t7->ShowMoveInList(uint16_t(move));
        }
        for (int i = 0; i < 4; ++i) frame(false, "");

        std::vector<int> list;
        // Frame count: render once with frame 0 to decode, then read it back through a forced frame clamp.
        AnimationPreview::forcedFrame = 0;
        frame(false, "");
        if (frames == "all" || frames.find('-') != std::string::npos) {
            int first = 1, last = 99999;
            if (frames != "all") Require(sscanf_s(frames.c_str(), "%d-%d", &first, &last) == 2, "FRAMES: A-B");
            for (int f = first; f <= last && f <= 20000; ++f) list.push_back(f);
        } else {
            size_t at = 0;
            while (at < frames.size()) { const auto next = frames.find(',', at); list.push_back(std::stoi(frames.substr(at, next - at))); if (next == std::string::npos) break; at = next + 1; }
        }
        clipFrames = AnimationPreview::lastFrames;
        clipFormat = AnimationPreview::lastFormat;
        // Collapsed preview: the form alone, once.
        if (previewOff) list.assign(1, 1), clipFrames = 1;
        Require(clipFrames > 0, "The preview shows no animation: " + AnimationPreview::lastError);
        for (int f : list) {
            if (f < 1 || f > clipFrames) continue;
            AnimationPreview::forcedFrame = f - 1;
            frame(false, "");
            frame(true, std::format("frame_{:04}.png", f));
        }
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(context);
    glfwDestroyWindow(window);
    glfwTerminate();
    Localization::Clear();
    std::cout << nlohmann::json({{"ok", written > 0}, {"written", written}, {"clip_frames", clipFrames}, {"format", clipFormat},
        {"directory", output.string()}}).dump() << '\n';
    return written > 0 ? 0 : 1;
}
