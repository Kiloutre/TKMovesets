#pragma once
#include "MovesetStructs.h"
#include <cstdint>
#include <array>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Per-moveset import profile: how a moveset is built at import, stored in the file's custom property list
// (TKMovesetProperty, between the header and the block list), never in the moveset data. Importers and the T7 <-> T8
// converters read it; the editor's "Import settings" action writes it. A file without a profile imports with the
// defaults, which match the behaviour before profiles existed. See docs/import-profile.md.
namespace ImportProfile {
// Property ids ('IP' + option). TKMovesetProperty_END (0) ends the list.
enum Id : int {
    IdVersion = 0x49500001, IdIdentity, IdCharacter, IdGender, IdStoryFlag, IdInHeat, IdFlagsClear,
    IdStoryMode, IdDlcStory, IdWaveIndex, IdBattleScript, IdSpecialModes, IdItem, IdBodyType, IdStage, IdStoryEffects,
    IdGlobalWrites,
};
inline constexpr int Version = 1;
inline constexpr int StoryFlagGameDecides = -1;
inline constexpr int ValueGameDecides = -1; // wave index / battle script value: leave the checks to the game

struct Profile {
    // Self character checks (T7 217, T8 220..227 and list checks): adopt the target character (the default: the
    // moveset acts as whoever it is imported onto) or act as `character` (this file's game numbering).
    enum class Identity : int { AdoptTarget = 0, Fixed = 1 } identity = Identity::AdoptTarget;
    int character = 0;
    // "Character is male/female" self checks (T7 583/584, T8 689/690): from the target, or always one gender.
    enum class Gender : int { FromTarget = 0, Male = 1, Female = 2 } gender = Gender::FromTarget;
    // T8 Story Flag (requirement 777). Game decides on a native T8 import; conversions to T7 use 0 (versus) for it.
    // 1..4 are story-chapter movesets (Jin: 4 = Devil Jin).
    int storyFlag = StoryFlagGameDecides;
    // T8 sources converted to T7, which has no Heat and no T8 flags: "Player is in Heat" (1028) and the flag-clear
    // checks (476 / 439 compared with 0).
    bool inHeat = false;
    bool flagsClear = true;
    // Context options from the full requirement / property review (docs/import-profile.md). Every default keeps the
    // behaviour from before the option existed: the game (native import) or the converter's rules decide.
    // Story mode checks (T7 558/559/636/811, T8 667/668/778/796/484..486/786/789/806/1014): or as outside story.
    enum class Toggle : int { Default = 0, Versus = 1 };
    Toggle storyMode = Toggle::Default;
    // DLC story battles (T8 801/802/803/810): or as outside DLC story.
    Toggle dlcStory = Toggle::Default;
    // Story wave / enemy index (T8 783..785, 1016) and battle script value (T8 769..771): or a fixed value.
    int waveIndex = ValueGameDecides;
    int battleScript = ValueGameDecides;
    // Arcade, episode and special modes (T7 556/560/634/765, T8 664/666/764..767/772): or all off.
    Toggle specialModes = Toggle::Default;
    // Customization item checks (T7 609/643, T8 718): or no item / every item.
    enum class Item : int { Default = 0, None = 1, All = 2 } item = Item::Default;
    // Body type self checks (T8 687 bear, 691 big character): or normal / big / bear (a bear is also big).
    enum class Body : int { Default = 0, Normal = 1, Big = 2, Bear = 3 } bodyType = Body::Default;
    // Stage checks (T7 573/574, T8 682/683/775): or any stage ("is stage X" never, "is not stage X" always).
    Toggle stage = Toggle::Default;
    // Story presentation / battle script properties (subtitles, HUD, cutscene VFX, story power crush, wave counter...)
    // and properties writing global game state: keep, or strip (they never fire).
    enum class Strip : int { Keep = 0, Strip = 1 };
    Strip storyEffects = Strip::Keep;
    Strip globalWrites = Strip::Keep;
    bool operator==(const Profile&) const = default;
};

// What the profile decides for one requirement row, in the moveset's own game numbering. nullopt: the profile does
// not decide (the game or the conversion rules do). `params`: the row's parameters (T7 rows have one).
enum class Game { T7, T8 };
std::optional<bool> RequirementOverride(const Profile& profile, Game game, uint32_t id, const std::array<uint32_t, 4>& params);
// Whether the profile strips an extra / start / end property (it never fires; conversions drop it).
bool StripsProperty(const Profile& profile, Game game, uint32_t id);
// The frame written into a stripped extra property: never reached by a move.
inline constexpr uint32_t StrippedFrame = 0x7FFF;

std::vector<TKMovesetProperty> ToProperties(const Profile& profile);
// Unknown ids are ignored (newer options); missing ones keep their defaults.
Profile FromProperties(std::span<const TKMovesetProperty> properties);
// A file's custom property list (before the block list); empty when it has none. `file` starts at the TKM2 header.
std::vector<TKMovesetProperty> ReadProperties(std::span<const Byte> file);
inline Profile Read(std::span<const Byte> file) { return FromProperties(ReadProperties(file)); }
// From a loaded file's header (its header region, up to the block list, is contiguous with it).
inline Profile Read(const TKMovesetHeader& header) {
    return Read(std::span<const Byte>(reinterpret_cast<const Byte*>(&header), header.block_list));
}
// The same file with another property list: header, block list and (possibly compressed) payload are kept byte for
// byte; only the list and the offsets after it change. Other properties (not the profile's) are kept.
std::vector<Byte> WithProfile(std::span<const Byte> file, const Profile& profile);
// Values the T7 <-> T8 conversion rules read through setting(NAME).
std::map<std::string, uint32_t> ConversionSettings(const Profile& profile);
// Human-readable summary (for listings and headless output).
std::string Describe(const Profile& profile);
}
