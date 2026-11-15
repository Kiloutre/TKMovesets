#include "AnimationReader.hpp"
#include <stdexcept>
#include <fstream>

#include "Animations.hpp"
#include "helpers.hpp"

#include "constants.h"

# define READ(addr, type) *(type*)(addr)

using namespace ByteswapHelpers;

namespace TAnimUtils
{
	char t5_anim_prefix[] = { 0x64, 0x00, 0x17, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x05, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x0B, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00 };
	char t5_anim_prefix_big_endian[] = { 0x00, 0x64, 0x00, 0x17, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x05, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x0B, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07 };

	void GetT5AnimPrefix(Byte* buffer, bool little_endian)
	{
		if (little_endian) {
			memcpy(buffer, t5_anim_prefix, sizeof(t5_anim_prefix));
		}
		else {
			memcpy(buffer, t5_anim_prefix_big_endian, sizeof(t5_anim_prefix_big_endian));
		}
	}

	unsigned int GetT5AnimPrefixSize()
	{
		return sizeof t5_anim_prefix;
	}


    namespace FromProcess
    {
        namespace {
            class ProcessReader
            {
                const GameProcess& m_process;
                gameAddr m_address;
                std::span<const Byte> m_prefix;
            public:
                ProcessReader(const GameProcess& process, gameAddr address, std::span<const Byte> prefix = {})
                    : m_process(process), m_address(address), m_prefix(prefix) {}
                size_t Size() const { return 256ULL << 20; }
                void Require(size_t offset, size_t length) const {
                    if (offset > Size() || length > Size() - offset)
                        throw std::out_of_range("Remote animation exceeds its budget");
                }
                template<class T> T Read(size_t offset, bool swapped = false) const {
                    Require(offset, sizeof(T));
                    T value;
                    if (offset < m_prefix.size()) {
                        if (sizeof(T) > m_prefix.size() - offset) throw std::out_of_range("Invalid animation prefix");
                        memcpy(&value, m_prefix.data() + offset, sizeof(T));
                    } else {
                        const auto relative = offset - m_prefix.size();
                        if (relative > UINT64_MAX - m_address ||
                            sizeof(T) > UINT64_MAX - (m_address + relative) ||
                            !m_process.readBytes(m_address + relative, &value, sizeof(T)))
                            throw std::out_of_range("Remote animation read failed");
                    }
                    if (swapped) {
                        auto bytes = reinterpret_cast<Byte*>(&value);
                        std::reverse(bytes, bytes + sizeof(T));
                    }
                    return value;
                }
            };

            uint64_t Size(const GameProcess& process, gameAddr address, uint16_t type,
                std::span<const Byte> prefix = {})
            {
                try {
                    ProcessReader input(process, address, prefix);
                    auto layout = ReadAnimationLayoutFrom(input, false);
                    return layout.type == type ? layout.size - prefix.size() : 0;
                } catch (const std::exception&) { return 0; }
            }
        }
        uint64_t getC8AnimSize(const GameProcess& process, gameAddr address) { return Size(process, address, 0xc8); }
        uint64_t get64AnimSize(const GameProcess& process, gameAddr address) { return Size(process, address, 0x64); }
        uint64_t get64AnimSize_LittleEndian(const GameProcess& process, gameAddr address) { return Size(process, address, 0x64); }
        uint64_t get64AnimSize_BigEndian(const GameProcess& process, gameAddr address) { return Size(process, address, 0x64); }
        uint64_t getT5_64AnimSize_LittleEndian(const GameProcess& process, gameAddr address)
        {
            return Size(process, address, 0x64, {reinterpret_cast<const Byte*>(t5_anim_prefix), sizeof(t5_anim_prefix)});
        }
        uint64_t getT5_64AnimSize_BigEndian(const GameProcess& process, gameAddr address)
        {
            return Size(process, address, 0x64, {reinterpret_cast<const Byte*>(t5_anim_prefix_big_endian), sizeof(t5_anim_prefix_big_endian)});
        }
    }

	namespace FromFile
	{
        bool GetAnimationInfos(const wchar_t* filename, s_animInfo& info)
        {
            info = {};
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            auto length = file.tellg();
            if (length < 8 || length > (256LL << 20)) return false;
            std::vector<Byte> bytes(static_cast<size_t>(length));
            file.seekg(0);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
            auto size = FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size());
            if (!size) return false;
            info.size = bytes.size();
            info.duration = FromMemory::Safe::GetAnimDuration(bytes.data(), bytes.size());
            info.hash = Helpers::CalculateCrc32(bytes.data(), bytes.size());
            return true;
        }

        int GetAnimationDuration(const wchar_t* filename)
        {
            s_animInfo info{};
            if (!GetAnimationInfos(filename, info) || info.duration > INT_MAX) return -1;
            return static_cast<int>(info.duration);
        }

	};
};