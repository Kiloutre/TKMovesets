#pragma once

#include "SharedMemory.h"

// Tag mode (retail T8, Practice): Tools > Extra writes a command, the injected loader acts on it.
enum TagCommand_ : int32_t
{
	TagCommand_None = 0,
	// Load P3 / P4 (tag_p3_fighter / tag_p4_fighter) at the next Practice entry and run Tag there
	TagCommand_Enable,
	// Remove the Tag hooks (the current match keeps its four fighters until Practice is left)
	TagCommand_Disable,
};

enum TagState_ : int32_t
{
	TagState_Off = 0,
	// Enabled, waiting for Practice to be (re)entered
	TagState_WaitingForPractice,
	// P3/P4 are being added to the running match (grow + per-slot setup): the tool's Load/Disable are greyed out
	TagState_Loading,
	// Tag is running
	TagState_Running,
	TagState_Error,
};

struct SharedMemT8 : SharedMemBase
{
	// Written by the tool: a command is taken when tag_request_id changes
	int32_t tag_request_id;
	int32_t tag_command;
	// Runtime fighter ids (the in-match character id, e.g. Jin 6, Reina 28)
	int32_t tag_p3_fighter;
	int32_t tag_p4_fighter;
	// Tag request without a pad (automation, tests): tag_trigger_team 0 = A, 1 = B, taken when tag_trigger_id changes
	int32_t tag_trigger_id;
	int32_t tag_trigger_team;
	// Scripted input for tests: tag_input_count masks (one per battle frame, the game's logical layout: up 1, down 2,
	// back 4, forward 8 relative to facing; buttons 1 0x4000, 2 0x8000, 3 0x1000, 4 0x2000) for the fighter in
	// tag_input_slot, then its move ids are logged for 90 frames. Taken when tag_input_id changes.
	int32_t tag_input_id;
	int32_t tag_input_slot;
	int32_t tag_input_count;
	uint32_t tag_input_masks[64];
	// Written by the loader
	int32_t tag_ack_id;
	int32_t tag_state;
	char tag_status[256];
};
