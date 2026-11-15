#pragma once
#include "GameProcess.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include <span>
#include <string>
#include <vector>

class ImporterT8;

// Direct live editing for retail T8: writes edited records into an already
// published moveset allocation instead of importing the document again.
// Only record contents may differ between the published and current document;
// table layouts, the source header and editor metadata must be unchanged.
namespace RetailLiveEdit {
struct Result {
    bool structural = false;   // a full import is required
    std::string reason;        // why, when structural
    size_t records = 0, bytes = 0;
};
// previous/current: complete TKM2 retail containers (as built by BuildSaveSnapshot).
// player/root: the player address and the allocation published for `previous`;
// offsets: the player-struct offsets from game_addresses.ini (ImporterT8::Offsets()).
Result Sync(GameProcess& process, gameAddr player, gameAddr root,
    std::span<const Byte> previous, std::span<const Byte> current, const RetailT8Runtime::PlayerOffsets& offsets);

// The editor's publish step (Import button and every live-edit revision): with a
// binding (root + snapshot published earlier on this slot), write changed records
// in place; structural changes, or no binding, import the document again.
struct Published {
    gameAddr root = 0;
    bool direct = false;   // written in place
    Result sync;           // direct-edit diagnostics (reason when a reimport was needed)
};
Published Apply(ImporterT8& importer, GameProcess& process, uint8_t slot,
    gameAddr boundRoot, const std::vector<Byte>* boundSnapshot, std::span<const Byte> document);
}
