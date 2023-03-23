#include "Editor_t7.hpp"

bool EditorT7::IsCommandGroupedCancelReference(const char* buffer)
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return command == constants[EditorConstants_GroupedCancelCommand];
}

int EditorT7::GetCommandInputSequenceID(const char* buffer)
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return (command & 0xFFFFFFFF) - constants[EditorConstants_InputSequenceCommandStart];
}

bool EditorT7::IsCommandInputSequence(const char* buffer)
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return (command & 0xFFFFFFFF) >= constants[EditorConstants_InputSequenceCommandStart];
}

bool EditorT7::IsPropertyThrowCameraRef(const char* buffer)
{
	uint32_t id = (uint32_t)strtol(buffer, nullptr, 16);
	if (id == 0x843C || id == 0x843D) {
		return true;
	}
	return false;
}
bool EditorT7::IsPropertyProjectileRef(const char* buffer)
{
	uint32_t id = (uint32_t)strtol(buffer, nullptr, 16);
	if (id == 0x820B) {
		return true;
	}
	return false;
}