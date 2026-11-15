#pragma once
#include "GameProcess.hpp"
#include <map>
#include <cstring>
#include <algorithm>

// Run the production importer against read-only live memory with all mutations
// redirected to local bytearrays. The source process handle needs no write rights.
class RetailImportShadow : public GameProcess {
    const GameProcess& m_source;
    gameAddr m_next = 0x700000000000ULL;
    std::map<gameAddr, std::vector<Byte>> m_bytes;
protected:
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override {
        auto it = m_bytes.upper_bound(address);
        if (it != m_bytes.begin()) {
            --it;
            if (address - it->first <= it->second.size() && size <= it->second.size() - (address - it->first)) {
                memcpy(data, it->second.data() + address - it->first, size); return true;
            }
        }
        if (!m_source.readBytes(address, data, size)) return false;
        for (const auto& [base, bytes] : m_bytes) {
            auto first = (std::max)(base, address), last = (std::min)(base + bytes.size(), address + size);
            if (first < last) memcpy(static_cast<Byte*>(data) + first - address, bytes.data() + first - base, last - first);
        }
        return true;
    }
    bool WriteMemoryRaw(gameAddr address, const void* data, size_t size) override {
        ++writes;
        auto it = m_bytes.upper_bound(address);
        if (it != m_bytes.begin()) {
            --it;
            if (address - it->first <= it->second.size() && size <= it->second.size() - (address - it->first)) {
                memcpy(it->second.data() + address - it->first, data, size); return true;
            }
        }
        const auto bytes = static_cast<const Byte*>(data);
        m_bytes[address] = {bytes, bytes + size}; return true;
    }
    gameAddr AllocateMemoryRaw(size_t size, bool) override {
        auto address = m_next; m_next += ((size + 0xffff) & ~size_t(0xffff)) + 0x10000;
        m_bytes[address].resize(size); return address;
    }
    bool ReleaseMemoryRaw(gameAddr address) override { return m_bytes.erase(address) != 0; }
    bool SuspendProcessRaw() override { return true; }
    bool ResumeProcessRaw() override { return true; }
public:
    std::vector<moduleEntry> GetModuleList() const override { return m_source.GetModuleList(); }
    bool CanPrepareGameResources() const override { return false; }
    size_t writes = 0;
    explicit RetailImportShadow(const GameProcess& source) : m_source(source) {
        mainModule = source.mainModule; processId = source.processId; status = source.status;
    }
};
