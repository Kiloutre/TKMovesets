#include "OpenRequests.hpp"
#include <commctrl.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

namespace OpenRequests {
namespace {
std::mutex mutex;
std::vector<std::wstring> pending;
constexpr const wchar_t* windowProperty = L"TKMovesets2.OpenRequests";
constexpr ULONG_PTR messageTag = 0x544B4D4F; // 'TKMO'
constexpr DWORD maxPathChars = 32768;

LRESULT CALLBACK ReceiveProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_COPYDATA) {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (data && data->dwData == messageTag && data->lpData && data->cbData % sizeof(wchar_t) == 0
            && data->cbData > 0 && data->cbData <= maxPathChars * sizeof(wchar_t)) {
            Queue(std::wstring(static_cast<const wchar_t*>(data->lpData), data->cbData / sizeof(wchar_t)));
            if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
            SetForegroundWindow(window);
            return TRUE;
        }
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

struct Search { DWORD pid; HWND found; };

BOOL CALLBACK FindListener(HWND window, LPARAM parameter)
{
    auto* search = reinterpret_cast<Search*>(parameter);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == search->pid && GetPropW(window, windowProperty)) { search->found = window; return FALSE; }
    return TRUE;
}
}

void Queue(std::wstring path)
{
    std::lock_guard lock(mutex);
    pending.push_back(std::move(path));
}

std::vector<std::wstring> Take()
{
    std::lock_guard lock(mutex);
    return std::exchange(pending, {});
}

bool SendToInstance(DWORD pid, const std::wstring& path)
{
    // The other instance may still be starting up: give its window a moment to appear.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    Search search{ pid, nullptr };
    while (true) {
        EnumWindows(FindListener, reinterpret_cast<LPARAM>(&search));
        if (search.found || std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!search.found) return false;

    AllowSetForegroundWindow(pid);
    COPYDATASTRUCT data{ messageTag, static_cast<DWORD>(path.size() * sizeof(wchar_t)), const_cast<wchar_t*>(path.data()) };
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(search.found, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 5000, &result) && result == TRUE;
}

void Listen(HWND window)
{
    if (!window) return;
    if (SetWindowSubclass(window, ReceiveProc, 1, 0)) SetPropW(window, windowProperty, reinterpret_cast<HANDLE>(1));
}
}
