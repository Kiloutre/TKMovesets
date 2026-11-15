#pragma once
#include "GameProcess.hpp"
#include <cstdint>
#include <span>
#include <vector>
#include <set>

namespace RetailT8Resources {
// Resource object of a loaded native animation (shared map first, then the character's map), 0 if absent.
gameAddr FindAnimationResource(const GameProcess& process, gameAddr container, uint32_t character, uint32_t instance);
// Retail 26155785: compiled animation vtable and EASTL's empty-bucket sentinel (module-relative).
constexpr gameAddr CompiledVtableRva = 0x75C32F0, EmptyBucketRva = 0x959ABD0;
// What the engine's PANM compile returns, built locally (.docs/retail-t8-compiled-anim-re.md): one image holding
// the PANM (samples are read in place), the 0xF0 object and its group/bone/track trees, with every pointer set for
// the image living at remote address `base` (16-aligned) in a game whose main module is at `module`.
// Throws for PANMs the game would reject and for unsupported keyframed/mode-list data.
struct CompiledAnimation { std::vector<Byte> image; gameAddr object; };
// Bones plus tracks visited while compiling one PANM (native body PANMs have under 100). Bounds the walk of
// FlatBuffer vectors that alias each other.
constexpr size_t MaxCompiledNodes = size_t(1) << 20;
CompiledAnimation BuildCompiledAnimation(std::span<const Byte> panm, gameAddr base, gameAddr module);

// Memory-only registration (.docs/retail-t8-memory-only-import.md). Game vtables used as data, module-relative:
// AnimResource (only its IsDone slot +0x28 is reached), ResourceRef (never called) and a side-effect-free
// function (`lea rax,[rcx+8]; ret`) for our shared-pointer control block.
constexpr gameAddr AnimResourceVtableRva = 0x85A59C0, ResourceRefVtableRva = 0x858A9C0, NoOpRva = 0xA6E230;
// The compiled animation plus a complete instance-map entry for id `id`: control block, finished load request,
// AnimResource (+0x190 object, +0x198 control block, +0x1A0 the PANM copy), ResourceRef and map node.
// The node's next/prev links are left for PublishAnimations.
struct RegisteredAnimation { std::vector<Byte> image; gameAddr object, resource, node; };
RegisteredAnimation BuildRegisteredAnimation(std::span<const Byte> panm, gameAddr base, gameAddr module, uint32_t id);
// Links uploaded entries into `map` (a domain's vector[0] instance map) through its buckets only: the readers'
// bucket walk finds them, the engine's list walks (teardown frees, rehash, readiness) never reach them. All
// reads, guards and writes go through `process`, so inside a WriteBatch nothing is published before Commit.
// Throws when the map is not populated or an id is already present. Returns the number published.
struct PublishedAnimation { uint32_t id; gameAddr node; };
size_t PublishAnimations(GameProcess& process, gameAddr map, std::span<const PublishedAnimation> entries);
// Content-derived id (top bit set) for a custom animation in `domain`, unused in domain 0x81 and `domain` and not in
// `taken`. `registered` is true when the id already resolves to our own entry holding the same PANM bytes (an earlier
// import), which is then reused instead of uploading again: re-importing does not grow memory.
struct AnimationId { uint32_t id; bool registered; };
AnimationId ChooseAnimationId(const GameProcess& process, gameAddr container, uint32_t domain, std::span<const Byte> panm,
    gameAddr module, const std::set<uint32_t>& taken = {});
// Makes the carrier resource play the PANM: builds the compiled animation locally, uploads it with plain writes
// (no code runs in the game) and points the carrier's +0x190 at it. All of it goes through `source`: inside a
// WriteBatch nothing is visible before Commit, and a failed or rolled-back import releases the upload. Once
// committed, the upload is never freed.
gameAddr AttachCustomAnimation(GameProcess& source, gameAddr resource, std::span<const Byte> panm, gameAddr owner = 0);
// Every attach records the carrier's original compiled object (per game process, in %TEMP%).
// Puts back the original animation of carriers attached for `owner` (a player) that are not in `keep`,
// when the resource still holds the object TKMovesets attached (writes through `source`, staged inside a WriteBatch).
// Returns the number restored.
size_t RestoreCarriers(GameProcess& source, gameAddr owner, std::span<const gameAddr> keep);
}
