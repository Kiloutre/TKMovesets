#pragma once

struct SharedMemBase
{
	// Contains the version of the moveset loader
	char moveset_loader_version[32];
	// Only load custom movesets if locked in
	bool locked_in;
	// Depending on the mode (persistent / online play), files might be sent over, movesets might be applied to different players, etc
	int onlineMode;
};