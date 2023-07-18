#pragma once

#include <map>
#include <string>
#include <iterator>
#include <algorithm>
#include <vector>
#include <stdlib.h>

#include "GameTypes.h"

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

#define offsetofVar(st, m) (offsetof(decltype(st), m))

#define BYTESWAP_INT64(x) _byteswap_uint64(x)
#define BYTESWAP_INT32(x) (((x & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x & 0xFF) << 24))
#define BYTESWAP_INT16(x) (((x & 0xFFFF) >> 8) | ((x & 0xFF) << 8))

#define DEREF_INT8(x) (*(int8_t*)(x))
#define DEREF_UINT8(x) (*(uint8_t*)(x))
#define DEREF_INT16(x) (*(int16_t*)(x))
#define DEREF_UINT16(x) (*(uint16_t*)(x))
#define DEREF_INT32(x) (*(int32_t*)(x))
#define DEREF_UINT32(x) (*(uint32_t*)(x))
#define DEREF_INT64(x) (*(int64_t*)(x))
#define DEREF_UINT64(x) (*(uint64_t*)(x))

// Helper class to iterate on struct pts
template<class T>
class StructIterator
{
public:
	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;

		Iterator(T* ptr) : m_ptr(ptr) {}

		reference operator[](int n) const { return m_ptr[n]; }
		reference operator*() const { return *m_ptr; }
		pointer operator->() { return m_ptr; }
		Iterator& operator++() { m_ptr++; return *this; }
		Iterator& operator+=(int n) { m_ptr += n; return *this; }
		Iterator operator++(int) { Iterator tmp = *this; (*this) += 1; return tmp; }
		Iterator operator+(int x) { return Iterator(&m_ptr[x]); }
		friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; }
		friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }

	private:
		T* m_ptr;
	};

	Iterator begin() const { return Iterator(&m_ptr[0]); }
	Iterator end() const { return Iterator(&m_ptr[m_size]); }
	uint64_t size() const { return m_size; }
	void set_size(uint64_t new_size) { m_size = new_size; }
	T* operator[](int n) const { return &m_ptr[n]; }

	StructIterator() { m_ptr = nullptr, m_size = 0; }
	StructIterator(void* ptr, size_t size) { Set(ptr, size); }
	StructIterator(void* base_ptr, void* ptr, size_t size) { Set(base_ptr, ptr, size); }
	StructIterator(void* base_ptr, uint64_t ptr, size_t size) { Set(base_ptr, ptr, size); }
	StructIterator(uint64_t base_ptr, void* ptr, size_t size) { Set(base_ptr, ptr, size); }
	StructIterator(uint64_t base_ptr, uint64_t ptr, size_t size) { Set(base_ptr, ptr, size); }
	void Set(void* ptr, size_t size) { m_ptr = (T*)ptr, m_size = size; }
	void Set(void* base_ptr, void* ptr, size_t size) { m_ptr = (T*)((uint64_t)base_ptr + (uint64_t)ptr), m_size = size; }
	void Set(void* base_ptr, uint64_t ptr, size_t size) { m_ptr = (T*)((uint64_t)base_ptr + ptr), m_size = size; }
	void Set(uint64_t base_ptr, void* ptr, size_t size) { m_ptr = (T*)(base_ptr + (uint64_t)ptr), m_size = size; }
	void Set(uint64_t base_ptr, uint64_t ptr, size_t size) { m_ptr = (T*)(base_ptr + ptr), m_size = size; }
private:
	T* m_ptr = nullptr;
	size_t m_size = 0;
};

// Helper class to avoid help optimize string comparisons in the editor
class FasterStringComp
{
private:
	std::string m_str;
	size_t m_size = 0;

public:
	FasterStringComp() { };
	FasterStringComp(const char* str) {
		m_str = std::string(str);
		m_size = m_str.size();
	};
	FasterStringComp(const std::string& str) {
		m_str = std::string(str);
		m_size = m_str.size();
	};
	void operator=(const char* str)
	{
		m_str = std::string(str);
		m_size = m_str.size();
	}

	const std::string& str() const { return m_str; }
	const char* c_str() const { return m_str.c_str(); }
	size_t size() const { return m_size; }

	//char operator[](int n) const { return m_str[n]; }
	bool operator==(const char* str) const { return *m_str.c_str() == *str && strcmp(m_str.c_str() + 1, str + 1) == 0; }
	bool operator==(const std::string& str) const { return m_size == str.size() && m_str == str; }

	std::string operator+(const char* str) const { return m_str + str; }
	friend std::string operator+(const char* str, const FasterStringComp& self) { return str + self.m_str; }
	std::string operator+(const std::string& str)const  { return m_str + str; }
	friend std::string operator+(const std::string& str, const FasterStringComp& self) { return str + self.m_str; }

	// Required for std::set, std::map sincethey are ordered
	bool operator>(const FasterStringComp& str) const { return m_str > str.str(); }
	bool operator<(const FasterStringComp& str) const { return m_str > str.str(); }
	//bool operator==(const FasterStringComp& str) const { return m_size == str.size() && m_str == str.str(); }

	bool startsWith(const char* prefix) const
	{
		const char* ptr = m_str.c_str();
		while (*prefix)
		{
			if (*prefix != *ptr) {
				return false;
			}
			++prefix;
			++ptr;
		}
		return true;
	}
	bool endsWith(const char* suffix) const
	{
		size_t suffixLen = strlen(suffix);
		if (suffixLen > m_size) {
			return false;
		}
		return strcmp(&m_str.c_str()[m_size - suffixLen], suffix) == 0;
	}
};

// Vector with unique elements, in order to keep insertion order
template < typename T >
class VectorSet : public std::vector<T> {
public:
	using iterator = typename std::vector<T>::iterator;
	using value_type = typename std::vector<T>::value_type;

	void push_back(const value_type& val) {
		auto end = this->end();
		auto it = ::std::find(this->begin(), end, val);
		if (it == end) {
			it = ::std::vector<T>::insert(end, val);
		}
	}
};

namespace ByteswapHelpers
{
	void SWAP_INT16(void* x);
	void SWAP_INT32(void* x);
	void SWAP_INT64(void* x);
}

class MovesetFile_BadSize : public std::exception{};
class MovesetFile_AllocationError : public std::exception{};
class MovesetFile_InvalidHeader : public std::exception{};

namespace Helpers
{

	// Converts a unicode-encoded string to UTF8
	std::string to_utf8(const std::wstring& ws);
	// Converts a UTF8-encoded string to Unicode
	std::wstring to_unicode(const std::string& s);
	// Quick wchar/wstring to string conversion
	std::string wstring_to_string(const std::wstring& ws);
	std::wstring string_to_wstring(const std::string& ws);

    // Make sure a value is divislbe by 8
    uint64_t align8Bytes(uint64_t value);

    // Make sure a file's writing cursor is divisble by 8
    void align8Bytes(std::ofstream& file);

	// Calculate a CRC32 from a list of data blocks. Always skip the first one.
	uint32_t CalculateCrc32(const std::vector<std::pair<Byte*, uint64_t>>& blocks);
	// Calculate a CRC32 from a block of data
	uint32_t CalculateCrc32(const Byte* data, uint64_t size);

    // Formats given date in 'hour:minutes day/month/year'
    std::string formatDateTime(uint64_t date, bool path_compatible=false);

	// Returns the current timestamp as a uint64
	uint64_t getCurrentTimestamp();

    // In a (moveset) list, can convert ptr members of every structure in the list into offsets
    void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount);

    // Returns true if a string ends with [suffix]
	template<typename T>
	bool endsWith(const T& str, const T& suffix)
	{
		if (str.length() < suffix.length())
			return false;
		size_t i = str.length() - suffix.length();

		for (auto c : suffix)
		{
			if (c != str[i]) {
				return false;
			}
			++i;
		}

		return true;
	}

	// Returns true if a string starts with [prefix]
	template<typename T>
	bool startsWith(const T& str, const T& prefix)
	{
		if (str.length() < prefix.length()) {
			return false;
		}

		for (size_t i = 0; prefix[i]; ++i)
		{
			if (prefix[i] != str[i]) {
				return false;
			}
		}

		return true;
	}

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(const std::wstring& filename);
    std::wstring getMovesetWNameFromFilename(const std::wstring& filename);

    // Returns true if one of the header strings is not properly formatted indicating a bad moveset file
    bool isHeaderStringMalformated(const char* string, size_t size);

    // Returns true if file exists
    bool fileExists(const wchar_t* name);
    bool fileExists(const char* name);

    // Update a crc32 with new data
    uint32_t crc32_update(const uint32_t(&table)[256], uint32_t initial, const void* buf, size_t len);
    // Create a table required for the crc32 generation
    void crc32_generate_table(uint32_t(&table)[256]);

	// Returns true if version1 is greater than version2
	bool VersionGreater(const char* version1, const char* version2);

	// Compares a memory area with a memory string such as '00 F7 E1 45 0? 45 ?? ?? 89'
	bool compare_memory_string(void* address, const char* bytesString);
	unsigned int get_memory_string_length(const char* bytesString);

	// Read a moveset file, allocate space or it, return allocated memory (or nullptr on error)
	Byte* ReadMovesetFile(const std::wstring& filename, uint64_t& size_out);

	// Returns true if the string is comprised of digits only. Ignores leading and trailing spaces.
	bool is_string_digits(const char* str);
}