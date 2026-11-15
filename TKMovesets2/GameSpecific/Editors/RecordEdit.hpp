#pragma once
#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>
#include "GameTypes.h"

// IDs supplied by the form are absolute record IDs. -1 inserts a record, reusing
// deleted bytes when available. Retained and deleted IDs must partition the old slice.
class RecordEdit
{
public:
    size_t start;
    size_t oldCount;
    size_t total;
    std::vector<int> retained;
    std::vector<int> sources;
    int difference;

    RecordEdit(size_t first, const std::vector<int>& ids, const std::set<int>& deleted, size_t total)
        : start(first), oldCount(0), total(total), retained(ids), sources(ids), difference(0)
    {
        if (ids.size() > INT_MAX || deleted.size() > INT_MAX || total > INT_MAX)
            throw std::invalid_argument("Record list is too large");
        std::set<int> unique;
        for (int id : ids) {
            if (id < -1 || (id >= 0 && (!unique.insert(id).second || deleted.contains(id))))
                throw std::invalid_argument("Duplicate or invalid record ID");
        }
        oldCount = unique.size() + deleted.size();
        if (first > total || oldCount > total - first)
            throw std::out_of_range("Record edit exceeds its list");
        for (int id : unique)
            if (id < 0 || static_cast<size_t>(id) < first || static_cast<size_t>(id) >= first + oldCount)
                throw std::out_of_range("Retained record is outside the edited slice");
        for (int id : deleted)
            if (id < 0 || static_cast<size_t>(id) < first || static_cast<size_t>(id) >= first + oldCount)
                throw std::out_of_range("Deleted record is outside the edited slice");
        difference = static_cast<int>(ids.size()) - static_cast<int>(oldCount);
        auto reusable = deleted.begin();
        for (int& source : sources)
            if (source == -1 && reusable != deleted.end()) source = *reusable++;
    }
    uint64_t Remap(uint64_t old) const
    {
        if (old == UINT64_MAX || old < start) return old;
        if (old >= start + oldCount) return old + difference;
        // A reference to the edited list's head continues to refer to its head.
        if (old == start && !sources.empty()) return start;
        auto it = std::find(retained.begin(), retained.end(), static_cast<int>(old));
        if (it != retained.end()) return start + (it - retained.begin());
        if (!sources.empty()) return start + (std::min<size_t>)(old - start, sources.size() - 1);
        const size_t remaining = total - oldCount;
        return remaining ? (std::min)(start, remaining - 1) : UINT64_MAX;
    }

    std::pair<uint64_t, uint64_t> RemapRange(uint64_t first, uint64_t count) const
    {
        if (!count) return {first, 0};
        if (first > total || count > total - first)
            throw std::out_of_range("Referenced record range exceeds its list");
        if (first <= start && first + count >= start + oldCount) {
            const auto amount = count + difference;
            return {amount ? first : 0, amount};
        }
        std::vector<uint64_t> positions;
        for (size_t i = first; i < first + count; ++i) {
            if (i >= start && i < start + oldCount &&
                std::find(retained.begin(), retained.end(), i) == retained.end()) continue;
            auto position = Remap(i);
            if (i >= start && i < start + oldCount)
                position = start + (std::find(retained.begin(), retained.end(), i) - retained.begin());
            positions.push_back(position);
        }
        if (positions.empty()) return {0, 0};
        std::sort(positions.begin(), positions.end());
        if (positions.back() - positions.front() + 1 != positions.size())
            throw std::invalid_argument("Reorder would split a referenced input sequence");
        return {positions.front(), positions.size()};
    }
};

template<class Index>
void RemapRecordIndex(Index& value, const RecordEdit& edit)
{
    if (value == static_cast<Index>(-1)) return;
    auto remapped = edit.Remap(value);
    if (remapped != UINT64_MAX && remapped > static_cast<uint64_t>((std::numeric_limits<Index>::max)()))
        throw std::out_of_range("Reference cannot represent its new record ID");
    value = static_cast<Index>(remapped);
}

struct RecordListCount
{
    uint64_t value;
    uint64_t field;
    size_t width;
};

struct RecordEditResult
{
    std::unique_ptr<Byte, decltype(&free)> bytes{nullptr, free};
    size_t size = 0;
    int difference = 0;
};

// Address slots are file positions of uint64_t offsets relative to the structure
// block. Count fields are explicit, so coincident empty lists stay distinguishable.
RecordEditResult BuildRecordEdit(std::span<const Byte> file, size_t recordSize,
    size_t listHead, size_t listCount, size_t countField, size_t countWidth,
    size_t first, const std::vector<int>& ids, const std::set<int>& deleted,
    size_t blockTable, size_t blockCount, size_t dataStart, size_t owningBlock,
    const std::vector<size_t>& structureAddressFields, size_t structuresStart,
    const std::vector<size_t>& movelistAddressFields, size_t targetMovelistField);
