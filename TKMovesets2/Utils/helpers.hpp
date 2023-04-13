#pragma once

#include <map>
#include <string>
#include <iterator>
#include <algorithm>
#include <vector>

#include "GameAddresses.h"

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

#define offsetofVar(st, m) (offsetof(decltype(st), m))

#define SWAP_INT32(x) (((x & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x & 0xFF) << 24))
#define SWAP_INT16(x) (((x & 0xFFFF) >> 8) | ((x & 0xFF) << 8))

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

	Iterator begin() { return Iterator(&m_ptr[0]); }
	Iterator end() { return Iterator(&m_ptr[m_size]); }
	uint64_t size() { return m_size; }
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
		auto it = ::std::find(this->begin(), this->end(), val);
		if (it == this->end()) {
			it = ::std::vector<T>::insert(this->end(), val);
		}
	}
};

namespace Helpers
{
	// Returns the size of an animation in bytes
	uint64_t GetAnimationSize(Byte* anim);
	uint64_t get64AnimSize_LittleEndian(Byte* anim);
	uint64_t get64AnimSize_BigEndian(Byte* anim);
	uint64_t get64AnimSize(Byte* anim);
	uint64_t getC8AnimSize(Byte* anim);

    // Make sure a value is divislbe by 8
    uint64_t align8Bytes(uint64_t value);

    // Make sure a file's writing cursor is divisble by 8
    void align8Bytes(std::ofstream& file);

	// Calcualte a crc32 from a list of data blocks. Always skip the first one.
	uint32_t CalculateCrc32(std::vector<std::pair<Byte*, uint64_t>>& blocks);

    // Formats given date in 'hour:minutes day/month/year'
    std::string formatDateTime(uint64_t date);

	// Returns the current timestamp as a uint64
	uint64_t getCurrentTimestamp();

    // In a (moveset) list, can convert ptr members of every structure in the list into offsets
    void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount);
    void convertPtrsToOffsets(void* listAddr, const std::map<gameAddr, uint64_t>& m, uint64_t struct_size, uint64_t amount);

    // Returns true if a string ends with [suffix]
    bool endsWith(const std::string& str, const std::string& suffix);
    // Returns true if a string starts with [prefix]
    bool startsWith(const std::string& str, const std::string& prefix);
    //bool endsWith(std::u32string str, std::u32string suffix);

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(const std::string& filename);

    // Returns true if one of the header strings is not properly formatted indicating a bad moveset file
    bool isHeaderStringMalformated(const char* string, size_t size);

    // Returns true if file exists
    bool fileExists(const char* name);

    // Update a crc32 with new data
    uint32_t crc32_update(uint32_t(&table)[256], uint32_t initial, const void* buf, size_t len);
    // Create a table required for the crc32 generation
    void crc32_generate_table(uint32_t(&table)[256]);
}