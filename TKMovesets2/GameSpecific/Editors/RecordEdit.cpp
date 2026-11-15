#include "RecordEdit.hpp"
#include <cstring>

namespace {
    constexpr size_t Budget = 256ULL << 20;
    uint64_t Read64(std::span<const Byte> bytes, size_t offset)
    {
        if (offset > bytes.size() || bytes.size() - offset < 8)
            throw std::out_of_range("Record metadata is truncated");
        uint64_t value;
        memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    void Write(void* bytes, size_t offset, uint64_t value, size_t width)
    {
        if (width != 4 && width != 8) throw std::invalid_argument("Unsupported metadata width");
        if (width == 4 && value > INT32_MAX) throw std::length_error("Record metadata overflow");
        memcpy(static_cast<Byte*>(bytes) + offset, &value, width);
    }
}

RecordEditResult BuildRecordEdit(std::span<const Byte> file, size_t stride,
    size_t head, size_t count, size_t countField, size_t countWidth,
    size_t first, const std::vector<int>& ids, const std::set<int>& deleted,
    size_t blocks, size_t blockCount, size_t dataStart, size_t owningBlock,
    const std::vector<size_t>& addressFields, size_t structuresStart,
    const std::vector<size_t>& movelistFields, size_t targetMovelistField)
{
    RecordEdit edit(first, ids, deleted, count);
    if (!stride || dataStart > file.size() || head < dataStart || head > file.size() || count > (file.size() - head) / stride ||
        blocks > file.size() || blockCount > (file.size() - blocks) / 8 || owningBlock >= blockCount)
        throw std::out_of_range("Record list exceeds the file");
    if (countWidth && (countField > head || countWidth > head - countField))
        throw std::out_of_range("Record count exceeds the file");

    const size_t begin = head + first * stride;
    const size_t oldEnd = begin + edit.oldCount * stride;
    const int64_t delta = static_cast<int64_t>(edit.difference) * static_cast<int64_t>(stride);
    size_t following = file.size();
    for (size_t i = owningBlock + 1; i < blockCount; ++i) {
        auto offset = Read64(file, blocks + 8 * i);
        if (offset > file.size() - dataStart) throw std::out_of_range("Invalid following block");
        if (dataStart + offset < oldEnd) throw std::out_of_range("Record edit crosses a block boundary");
        following = dataStart + offset;
        break;
    }
    if (oldEnd > following) throw std::out_of_range("Record edit crosses its section");
    const int64_t moved = static_cast<int64_t>(following) + delta;
    if (moved < 0 || moved > Budget) throw std::length_error("Edited file exceeds its budget");
    const size_t next = following == file.size() ? static_cast<size_t>(moved)
        : (static_cast<size_t>(moved) + 7) & ~size_t(7);
    const int64_t total = static_cast<int64_t>(file.size()) + static_cast<int64_t>(next) - static_cast<int64_t>(following);
    if (total < 0 || total > Budget) throw std::length_error("Edited file exceeds its budget");
    RecordEditResult result;
    result.size = static_cast<size_t>(total);
    result.difference = edit.difference;
    result.bytes.reset(static_cast<Byte*>(calloc(1, result.size)));
    if (!result.bytes) throw std::bad_alloc();
    auto out = result.bytes.get();
    memcpy(out, file.data(), begin);
    for (size_t i = 0; i < edit.sources.size(); ++i)
        if (edit.sources[i] >= 0)
            memcpy(out + begin + i * stride, file.data() + head + edit.sources[i] * stride, stride);
    const size_t newEnd = begin + ids.size() * stride;
    memcpy(out + newEnd, file.data() + oldEnd, following - oldEnd);
    memcpy(out + next, file.data() + following, file.size() - following);

    if (countWidth) Write(out, countField, static_cast<int64_t>(count) + edit.difference, countWidth);
    for (size_t slot : addressFields) {
        auto relative = Read64(file, slot);
        auto absolute = structuresStart + relative;
        // Equal starts belong to the following table only when its metadata slot
        // follows the edited table's count field.
        if (absolute > begin && absolute < oldEnd)
            Write(out, slot, begin - structuresStart, 8);
        else if (absolute >= oldEnd && (absolute > begin || slot > countField))
            Write(out, slot, static_cast<int64_t>(relative) + delta, 8);
    }
    for (size_t i = owningBlock + 1; i < blockCount; ++i) {
        auto offset = Read64(file, blocks + i * 8);
        Write(out, blocks + i * 8, offset + next - following, 8);
    }
    for (size_t slot : movelistFields) {
        if (slot > file.size() || file.size() - slot < 4) throw std::out_of_range("Movelist field is truncated");
        uint32_t relative;
        memcpy(&relative, file.data() + slot, 4);
        auto absolute = structuresStart + relative;
        if (slot != targetMovelistField && (absolute > begin || (absolute == begin && slot > targetMovelistField)))
            Write(out, slot, static_cast<int64_t>(relative) + delta, 4);
    }
    return result;
}
