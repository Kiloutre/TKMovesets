#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Movesets the GUI was asked to open from outside (double-clicked in Explorer).
// A request made while another instance runs is forwarded to that instance's main window.
namespace OpenRequests {
// Queues a moveset file for this process's GUI to open
void Queue(std::wstring path);
// Returns and clears the queued requests
std::vector<std::wstring> Take();
// Hands the request to the running instance with this pid, returns false if it could not be reached
bool SendToInstance(DWORD pid, const std::wstring& path);
// Makes this window receive requests from other instances
void Listen(HWND window);
}
