#pragma once

#include <cstddef>
#include <cstdint>

#include "GameTypes.h"

// Retail Tekken 8 moveset records (.motbin as loaded in memory; build 26155785).
// The CNT-era layout is in Structs_t8cnt.h; the two differ (header 0x318 vs 0x310, Move 0x448, Projectile 0xE0).
//
// Evidence lives in the RE project (G:\Dev\Retroingenierie\Tekken8 cnt):
//   docs/formats/tkdata.md section 3 (header, encrypted move blocks) and 3.1 (hitboxes, reactions),
//   findings log 2026-11-14 "Retail move hitbox fields verified" (hit-line builder 0x1418C3EE0, live test 0x141913960)
//   and "Reaction move ids and hitbox layout" (reaction picker 0x1418FE330).
// Fields named _0xNN are not identified. In memory every pointer is absolute; on disk the header table pointers
// are relative to the end of the header and record pointers are row indices (see RetailDiskMoveset.cpp).
namespace StructsT8
{
#pragma pack(push, 1)
	// One of the six encrypted move values: value/key pair (EditorT8Retail::Decode), then four u32 slots, one of
	// which keeps a plain copy that the game cross-checks.
	struct EncryptedValue
	{
		uint64_t value;
		uint64_t key;
		uint32_t check_slots[4];
	};

	struct MovesetTable
	{
		gameAddr address;
		uint64_t count;
	};

	struct MovesetHeader
	{
		uint16_t skip_animation_lookup;   // 0: move +0x50 holds a body animation list index on disk
		uint8_t  relocated;               // set by the load step
		uint8_t  _0x3;
		uint32_t compile_date;
		char     signature[8];            // "TEK"
		gameAddr strings[4];              // character name, creator, date, full date
		uint16_t orig_aliases[60];
		uint16_t current_aliases[60];
		uint16_t unknown_aliases[32];
		uint32_t previous_identity;       // IdentityCode(id - 1): n << 16 | (0x10000 - n)
		uint32_t identity;                // IdentityCode(id)
		gameAddr reactions;
		gameAddr _0x170;                  // runtime, cleared on load
		uint64_t reactions_count;
		MovesetTable requirements;
		MovesetTable hit_conditions;
		MovesetTable projectiles;
		MovesetTable pushbacks;
		MovesetTable pushback_extra;
		MovesetTable cancels;
		MovesetTable group_cancels;
		MovesetTable cancel_extra;
		MovesetTable extra_properties;
		MovesetTable start_properties;
		MovesetTable end_properties;
		MovesetTable moves;
		MovesetTable voiceclips;
		MovesetTable input_sequences;
		MovesetTable inputs;
		MovesetTable parry;
		MovesetTable cameras;
		MovesetTable throws;
		MovesetTable dialogues;
		gameAddr runtime_slots[13];       // 0x2B0..0x310, runtime resources, cleared on load
	};

	struct Reactions
	{
		gameAddr pushbacks[7];            // front, backturned, left_side, right_side, front_counterhit, downed, block
		uint16_t direction[6];            // front, back, left_side, right_side, front_counterhit, downed
		uint16_t rotation[4];             // front, back, left_side, right_side
		int16_t  vertical_pushback;
		uint16_t downed_rotation;         // 0 in every retail file; not a move id
		uint16_t moveid[14];              // standing .. downed; the only move ids the reaction picker reads
		uint16_t _0x6C[2];                // 0 in every retail file
	};

	struct Requirement
	{
		uint32_t condition;
		uint32_t params[4];
	};

	struct HitCondition
	{
		gameAddr requirements;
		uint32_t damage;
		uint32_t _0xC;
		gameAddr reactions;
	};

	struct Projectile
	{
		uint32_t vfx_id;
		uint32_t _0x4[11];
		uint32_t _0x30;
		uint32_t _0x34;
		uint32_t _0x38;
		uint32_t hit_level;
		uint32_t _0x40[10];
		uint32_t voiceclip[3];            // {category, 0, id}
		uint32_t _0x74[7];
		gameAddr hit_condition;
		gameAddr cancel;
		uint32_t _0xA0;
		uint32_t _0xA4;
		uint32_t _0xA8;
		uint32_t _0xAC;                   // load step: 0 -> 2 * _0x30
		uint32_t _0xB0;                   // load step: 0 -> _0x34
		uint32_t _0xB4;
		uint32_t _0xB8;
		uint32_t _0xBC;
		uint32_t _0xC0[3];
		uint32_t _0xCC;
		uint32_t _0xD0[4];
	};

	struct Pushback
	{
		uint16_t duration;
		uint16_t displacement;
		uint32_t extra_count;
		gameAddr extra;
	};

	struct PushbackExtra
	{
		uint16_t value;
	};

	struct Cancel
	{
		uint64_t command;
		gameAddr requirements;
		gameAddr extra;
		uint32_t window_start;
		uint32_t window_end;
		uint32_t starting_frame;
		uint16_t move_id;
		uint16_t option;                  // derived by the load step
	};

	struct CancelExtra
	{
		uint32_t value;
	};

	struct ExtraMoveProperty
	{
		uint32_t frame;
		uint32_t _0x4;
		gameAddr requirements;
		uint32_t id;
		uint32_t params[5];
	};

	struct OtherMoveProperty
	{
		gameAddr requirements;
		uint32_t id;
		uint32_t params[5];
	};

	struct Voiceclip
	{
		uint32_t category;
		uint32_t _0x4;
		uint32_t id;
	};

	struct InputSequence
	{
		uint16_t window;
		uint16_t input_count;
		uint32_t _0x4;
		gameAddr inputs;
	};

	struct Input
	{
		uint32_t direction;
		uint32_t buttons;
	};

	struct Parry
	{
		uint32_t value;
	};

	struct CameraData
	{
		uint32_t probability;
		uint16_t type;
		uint16_t left_camera;
		uint16_t right_camera;
		uint16_t rotation;
	};

	struct ThrowCamera
	{
		uint64_t side;
		gameAddr camera;
	};

	struct Dialogue
	{
		uint16_t type;
		uint16_t id;
		uint32_t _0x4;
		gameAddr requirements;
		uint32_t voice_key;
		int32_t  facial_animation;
	};

	struct Hitbox
	{
		uint32_t start;                   // first active frame of this slot
		uint32_t last;                    // last active frame
		uint32_t location;                // byte 0 limb A, byte 1 limb B (0 = sweep of A from the previous frame)
		float    radius;                  // 0 = character default
		float    vertical_height;         // > 0: vertical segment of that height centred on A
		float    offset_a[3];
		float    offset_b[3];
		uint32_t starts_off;              // low byte copied to the player's per-slot off flag at move start
	};

	struct Move
	{
		EncryptedValue name_key;
		EncryptedValue anim_key;
		gameAddr name_addr;
		gameAddr anim_name_addr;
		uint64_t anim_addr;               // disk: body list index; memory: animation handle
		EncryptedValue vuln;
		EncryptedValue hit_level;
		gameAddr cancels;
		gameAddr cancels_2;
		int32_t  cancel_gate_2;           // < 1: cancels_2 unused
		uint32_t _0xAC;
		gameAddr cancels_3;
		int32_t  cancel_gate_3;
		uint32_t _0xBC;
		gameAddr cancels_4;
		int32_t  cancel_gate_4;
		uint16_t transition;
		uint16_t end_rotation;
		EncryptedValue character_id;      // IdentityCode(id - 1)
		EncryptedValue move_key;          // Kamui(CODE_name)
		gameAddr hit_conditions;
		int16_t  damage_override;
		uint16_t _0x11A;
		uint32_t animation_length_adjuster;
		uint32_t animation_length;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		gameAddr voiceclip;
		gameAddr extra_properties;
		gameAddr start_properties;
		gameAddr end_properties;
		uint32_t _0x150;
		uint32_t _0x154;
		uint32_t active_start;            // overall active window; 0 = no hitboxes
		uint32_t active_end;
		Hitbox   hitboxes[8];
		uint16_t collision;
		uint16_t distance;
		uint32_t _0x2E4[89];              // native defaults repeat every 0x2C from 0x2E4 (8 slots), unidentified
	};
#pragma pack(pop)

	static_assert(sizeof(EncryptedValue) == 0x20);
	static_assert(sizeof(MovesetTable) == 0x10);

	static_assert(sizeof(MovesetHeader) == 0x318);
	static_assert(offsetof(MovesetHeader, compile_date) == 0x4);
	static_assert(offsetof(MovesetHeader, signature) == 0x8);
	static_assert(offsetof(MovesetHeader, strings) == 0x10);
	static_assert(offsetof(MovesetHeader, orig_aliases) == 0x30);
	static_assert(offsetof(MovesetHeader, current_aliases) == 0xA8);
	static_assert(offsetof(MovesetHeader, unknown_aliases) == 0x120);
	static_assert(offsetof(MovesetHeader, previous_identity) == 0x160);
	static_assert(offsetof(MovesetHeader, identity) == 0x164);
	static_assert(offsetof(MovesetHeader, reactions) == 0x168);
	static_assert(offsetof(MovesetHeader, _0x170) == 0x170);
	static_assert(offsetof(MovesetHeader, reactions_count) == 0x178);
	static_assert(offsetof(MovesetHeader, requirements) == 0x180);
	static_assert(offsetof(MovesetHeader, moves) == 0x230);
	static_assert(offsetof(MovesetHeader, dialogues) == 0x2A0);
	static_assert(offsetof(MovesetHeader, runtime_slots) == 0x2B0);

	static_assert(sizeof(Reactions) == 0x70);
	static_assert(offsetof(Reactions, direction) == 0x38);
	static_assert(offsetof(Reactions, rotation) == 0x44);
	static_assert(offsetof(Reactions, vertical_pushback) == 0x4C);
	static_assert(offsetof(Reactions, downed_rotation) == 0x4E);
	static_assert(offsetof(Reactions, moveid) == 0x50);
	static_assert(offsetof(Reactions, _0x6C) == 0x6C);

	static_assert(sizeof(Requirement) == 0x14);
	static_assert(sizeof(HitCondition) == 0x18);
	static_assert(offsetof(HitCondition, damage) == 0x8);
	static_assert(offsetof(HitCondition, reactions) == 0x10);

	static_assert(sizeof(Projectile) == 0xE0);
	static_assert(offsetof(Projectile, hit_level) == 0x3C);
	static_assert(offsetof(Projectile, voiceclip) == 0x68);
	static_assert(offsetof(Projectile, hit_condition) == 0x90);
	static_assert(offsetof(Projectile, cancel) == 0x98);
	static_assert(offsetof(Projectile, _0xAC) == 0xAC);
	static_assert(offsetof(Projectile, _0xCC) == 0xCC);

	static_assert(sizeof(Pushback) == 0x10);
	static_assert(offsetof(Pushback, extra) == 0x8);
	static_assert(sizeof(PushbackExtra) == 0x2);
	static_assert(sizeof(Cancel) == 0x28);
	static_assert(offsetof(Cancel, requirements) == 0x8);
	static_assert(offsetof(Cancel, extra) == 0x10);
	static_assert(offsetof(Cancel, window_start) == 0x18);
	static_assert(offsetof(Cancel, starting_frame) == 0x20);
	static_assert(offsetof(Cancel, move_id) == 0x24);
	static_assert(offsetof(Cancel, option) == 0x26);
	static_assert(sizeof(CancelExtra) == 0x4);
	static_assert(sizeof(ExtraMoveProperty) == 0x28);
	static_assert(offsetof(ExtraMoveProperty, requirements) == 0x8);
	static_assert(offsetof(ExtraMoveProperty, id) == 0x10);
	static_assert(offsetof(ExtraMoveProperty, params) == 0x14);
	static_assert(sizeof(OtherMoveProperty) == 0x20);
	static_assert(offsetof(OtherMoveProperty, id) == 0x8);
	static_assert(offsetof(OtherMoveProperty, params) == 0xC);
	static_assert(sizeof(Voiceclip) == 0xC);
	static_assert(sizeof(InputSequence) == 0x10);
	static_assert(offsetof(InputSequence, input_count) == 0x2);
	static_assert(offsetof(InputSequence, inputs) == 0x8);
	static_assert(sizeof(Input) == 0x8);
	static_assert(sizeof(Parry) == 0x4);
	static_assert(sizeof(CameraData) == 0xC);
	static_assert(offsetof(CameraData, rotation) == 0xA);
	static_assert(sizeof(ThrowCamera) == 0x10);
	static_assert(offsetof(ThrowCamera, camera) == 0x8);
	static_assert(sizeof(Dialogue) == 0x18);
	static_assert(offsetof(Dialogue, requirements) == 0x8);
	static_assert(offsetof(Dialogue, voice_key) == 0x10);
	static_assert(offsetof(Dialogue, facial_animation) == 0x14);

	static_assert(sizeof(Hitbox) == 0x30);
	static_assert(offsetof(Hitbox, location) == 0x8);
	static_assert(offsetof(Hitbox, radius) == 0xC);
	static_assert(offsetof(Hitbox, vertical_height) == 0x10);
	static_assert(offsetof(Hitbox, offset_a) == 0x14);
	static_assert(offsetof(Hitbox, offset_b) == 0x20);
	static_assert(offsetof(Hitbox, starts_off) == 0x2C);

	static_assert(sizeof(Move) == 0x448);
	static_assert(offsetof(Move, anim_key) == 0x20);
	static_assert(offsetof(Move, name_addr) == 0x40);
	static_assert(offsetof(Move, anim_name_addr) == 0x48);
	static_assert(offsetof(Move, anim_addr) == 0x50);
	static_assert(offsetof(Move, vuln) == 0x58);
	static_assert(offsetof(Move, hit_level) == 0x78);
	static_assert(offsetof(Move, cancels) == 0x98);
	static_assert(offsetof(Move, cancels_2) == 0xA0);
	static_assert(offsetof(Move, cancel_gate_2) == 0xA8);
	static_assert(offsetof(Move, cancels_3) == 0xB0);
	static_assert(offsetof(Move, cancel_gate_3) == 0xB8);
	static_assert(offsetof(Move, cancels_4) == 0xC0);
	static_assert(offsetof(Move, cancel_gate_4) == 0xC8);
	static_assert(offsetof(Move, transition) == 0xCC);
	static_assert(offsetof(Move, end_rotation) == 0xCE);
	static_assert(offsetof(Move, character_id) == 0xD0);
	static_assert(offsetof(Move, move_key) == 0xF0);
	static_assert(offsetof(Move, hit_conditions) == 0x110);
	static_assert(offsetof(Move, damage_override) == 0x118);
	static_assert(offsetof(Move, animation_length_adjuster) == 0x11C);
	static_assert(offsetof(Move, animation_length) == 0x120);
	static_assert(offsetof(Move, airborne_start) == 0x124);
	static_assert(offsetof(Move, airborne_end) == 0x128);
	static_assert(offsetof(Move, ground_fall) == 0x12C);
	static_assert(offsetof(Move, voiceclip) == 0x130);
	static_assert(offsetof(Move, extra_properties) == 0x138);
	static_assert(offsetof(Move, start_properties) == 0x140);
	static_assert(offsetof(Move, end_properties) == 0x148);
	static_assert(offsetof(Move, _0x150) == 0x150);
	static_assert(offsetof(Move, active_start) == 0x158);
	static_assert(offsetof(Move, active_end) == 0x15C);
	static_assert(offsetof(Move, hitboxes) == 0x160);
	static_assert(offsetof(Move, collision) == 0x2E0);
	static_assert(offsetof(Move, distance) == 0x2E2);
	static_assert(offsetof(Move, _0x2E4) == 0x2E4);
}
