#include "GameProcess.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

bool GameProcess::ReadMemoryRaw(gameAddr address, void* data, size_t size) const {
    SIZE_T count = 0;
    return ReadProcessMemory(m_processHandle, reinterpret_cast<const void*>(address), data, size, &count) && count == size;
}
bool GameProcess::WriteMemoryRaw(gameAddr address, const void* data, size_t size) {
    SIZE_T count = 0;
    return WriteProcessMemory(m_processHandle, reinterpret_cast<void*>(address), data, size, &count) && count == size;
}
gameAddr GameProcess::AllocateMemoryRaw(size_t size, bool executable) {
    return reinterpret_cast<gameAddr>(VirtualAllocEx(m_processHandle, nullptr, size, MEM_COMMIT | MEM_RESERVE,
        executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE));
}
bool GameProcess::ReleaseMemoryRaw(gameAddr address) {
    return VirtualFreeEx(m_processHandle, reinterpret_cast<void*>(address), 0, MEM_RELEASE);
}
bool GameProcess::SuspendProcessRaw() {
    std::lock_guard lock(m_resourceMutex);
    using Function = LONG(NTAPI*)(HANDLE);
    auto function = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSuspendProcess"));
    if (m_suspensionHandle && WaitForSingleObject(m_suspensionHandle, 0) == WAIT_OBJECT_0) {
        // Left over from a process that has exited since: nothing to resume anymore
        CloseHandle(m_suspensionHandle); m_suspensionHandle = nullptr;
    }
    if (m_suspensionHandle) return false;
    // SYNCHRONIZE lets a failed resume tell whether the process has exited
    auto handle = OpenProcess(PROCESS_SUSPEND_RESUME | SYNCHRONIZE, FALSE, processId);
    if (!function || !handle) { if (handle) CloseHandle(handle); return false; }
    auto result = function(handle);
    if (result < 0) { CloseHandle(handle); return false; }
    m_suspensionHandle = handle; return true;
}
bool GameProcess::ResumeProcessRaw() {
    std::lock_guard lock(m_resourceMutex);
    using Function = LONG(NTAPI*)(HANDLE);
    auto function = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtResumeProcess"));
    if (!function || !m_suspensionHandle) return false;
    auto result = function(m_suspensionHandle);
    if (result < 0) {
        // An exited process has nothing left to resume: release the handle so later suspensions are not refused forever
        if (WaitForSingleObject(m_suspensionHandle, 0) == WAIT_OBJECT_0) {
            CloseHandle(m_suspensionHandle); m_suspensionHandle = nullptr;
        }
        return false;
    }
    CloseHandle(m_suspensionHandle); m_suspensionHandle = nullptr; return true;
}
GameProcess::PauseGuard::PauseGuard(GameProcess& process) : m_process(process) {
    if (!m_process.SuspendProcessRaw()) throw std::runtime_error("Cannot pause game for atomic retail moveset publication");
}
GameProcess::PauseGuard::~PauseGuard() { if (!m_process.ResumeProcessRaw()) m_process.ResumeProcessRaw(); }
bool GameProcess::ExpectBytes(gameAddr address, const void* bytes, size_t size) {
    if (!bytes || !size || size > UINT64_MAX - address) return false;
    const auto start = static_cast<const Byte*>(bytes);
    if (auto batch = ActiveBatch()) {
        batch->m_expectations.push_back({address, {start, start + size}});
        return true;
    }
    std::vector<Byte> current(size);
    return ReadMemoryRaw(address, current.data(), size) && !memcmp(current.data(), bytes, size);
}
bool GameProcess::ReleaseNow(gameAddr address) {
    std::lock_guard lock(m_resourceMutex);
    if (!ReleaseMemoryRaw(address)) return false;
    std::erase_if(allocatedMemory, [&](const auto& entry) { return entry.first == address; });
    if (auto batch = ActiveBatch()) std::erase_if(batch->m_newAllocations, [&](const auto& entry) { return entry.first == address; });
    return true;
}
void GameProcess::ReapRemoteThreads() {
    std::lock_guard lock(m_resourceMutex);
    for (size_t i = 0; i < m_pendingThreads.size();) {
        auto pending = m_pendingThreads[i];
        if (WaitForSingleObject(pending.handle, 0) != WAIT_OBJECT_0) { ++i; continue; }
        CloseHandle(pending.handle);
        m_pendingThreads.erase(m_pendingThreads.begin() + i);
        if (pending.releaseAddress) freeMem(pending.releaseAddress);
    }
}
GameProcess::WriteBatch::WriteBatch(GameProcess& process) : m_process(process), m_generation(process.m_generation) {
    std::lock_guard lock(process.m_resourceMutex);
    if (process.m_activeWriteBatch) throw std::logic_error("Remote write batches cannot be nested");
    process.m_activeWriteBatch = this;
}
GameProcess::WriteBatch::~WriteBatch() {
    {
        std::lock_guard lock(m_process.m_resourceMutex);
        if (m_process.m_activeWriteBatch == this) m_process.m_activeWriteBatch = nullptr;
    }
    if (m_committed || !m_result.rollbackComplete || m_generation != m_process.m_generation) return;
    for (const auto& [address, size] : m_newAllocations) m_process.ReleaseNow(address);
}
bool GameProcess::WriteBatch::IsPrivate(gameAddr address, size_t size) const {
    for (const auto& [base, length] : m_newAllocations)
        if (address >= base && address - base <= length && size <= length - (address - base)) return true;
    return false;
}
bool GameProcess::WriteBatch::Stage(gameAddr address, const void* data, size_t size) {
    if (m_failed || m_finished || m_generation != m_process.m_generation ||
        (!data && size) || size > UINT64_MAX - address) {
        m_failed = true; SetLastError(ERROR_INVALID_PARAMETER); return false;
    }
    if (!size) return true;
    const auto bytes = static_cast<const Byte*>(data);
    m_operations.push_back({address, {bytes, bytes + size}});
    return true;
}
bool GameProcess::WriteBatch::Overlay(gameAddr address, void* data, size_t size) const {
    if (size > UINT64_MAX - address) return false;
    for (const auto& op : m_operations) {
        const auto first = (std::max)(address, op.address);
        const auto last = (std::min)(address + size, op.address + op.bytes.size());
        if (first < last) memcpy(static_cast<Byte*>(data) + first - address, op.bytes.data() + first - op.address, last - first);
    }
    return true;
}
GameProcess::WriteResult GameProcess::WriteBatch::Commit() {
    if (m_finished) throw std::logic_error("Remote write batch already finished");
    {
        std::lock_guard lock(m_process.m_resourceMutex);
        m_finished = true; m_process.m_activeWriteBatch = nullptr;
    }
    if (m_failed || m_generation != m_process.m_generation) {
        m_result.error = ERROR_INVALID_HANDLE; return m_result;
    }
    {
        std::lock_guard lock(m_process.m_resourceMutex);
        m_process.m_pinnedAllocations.reserve(m_process.m_pinnedAllocations.size() + m_newAllocations.size());
    }
    for (const auto& expected : m_expectations) {
        std::vector<Byte> current(expected.bytes.size());
        if (!m_process.ReadMemoryRaw(expected.address, current.data(), current.size()) || current != expected.bytes) {
            m_result.error = ERROR_RETRY; return m_result;
        }
    }
    for (auto& op : m_operations) {
        op.privateUpload = IsPrivate(op.address, op.bytes.size());
        if (op.privateUpload) continue;
        op.before.resize(op.bytes.size());
        if (!m_process.ReadMemoryRaw(op.address, op.before.data(), op.before.size()) || m_generation != m_process.m_generation) {
            m_result.error = ERROR_PARTIAL_COPY; return m_result;
        }
    }
    // Upload private data completely before publishing any shared pointer/metadata.
    for (const auto& op : m_operations) {
        if (!op.privateUpload) continue;
        if (m_generation != m_process.m_generation || !m_process.WriteMemoryRaw(op.address, op.bytes.data(), op.bytes.size())) {
            m_result.error = ERROR_PARTIAL_COPY; return m_result;
        }
        std::vector<Byte> verified(op.bytes.size());
        if (!m_process.ReadMemoryRaw(op.address, verified.data(), verified.size()) || verified != op.bytes) {
            m_result.error = ERROR_PARTIAL_COPY; return m_result;
        }
    }
    for (size_t i = 0; i < m_operations.size(); ++i) {
        const auto& op = m_operations[i];
        if (op.privateUpload) continue;
        if (m_generation == m_process.m_generation && m_process.WriteMemoryRaw(op.address, op.bytes.data(), op.bytes.size())) continue;
        m_result.error = ERROR_PARTIAL_COPY;
        for (size_t j = i + 1; j-- > 0;) {
            const auto& undo = m_operations[j];
            if (undo.privateUpload) continue;
            if (m_generation != m_process.m_generation || !m_process.WriteMemoryRaw(undo.address, undo.before.data(), undo.before.size()))
                m_result.rollbackComplete = false;
        }
        if (!m_result.rollbackComplete && m_generation == m_process.m_generation) {
            std::lock_guard lock(m_process.m_resourceMutex);
            for (auto [address, size] : m_newAllocations) m_process.m_pinnedAllocations.push_back(address);
        }
        return m_result;
    }
    m_committed = m_result.ok = true;
    return m_result;
}
