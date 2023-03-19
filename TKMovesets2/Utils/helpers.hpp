#pragma once

#include <map>
#include <string>
#include <iterator>

#include "GameAddresses.h"

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

namespace Helpers
{
    // Make sure a value is divislbe by 8
    uint64_t align8Bytes(uint64_t value);

    // Make sure a file's cursor is divisble by 8
    void align8Bytes(std::ofstream& file);

    // Gets the current date format in 'hour:minutes day/month/year'
    std::string currentDateTime(uint64_t date);

    // In a (moveset) list, can convert ptr members of every structure in the list into offsets
    void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount);
    void convertPtrsToOffsets(void* listAddr, std::map<gameAddr, uint64_t> m, uint64_t struct_size, uint64_t amount);

    // Returns true if a string ends with [suffix]
    bool endsWith(std::string str, std::string suffix);
    // Returns true if a string starts with [prefix]
    bool startsWith(std::string str, std::string prefix);
    //bool endsWith(std::u32string str, std::u32string suffix);

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(std::string filename);

    // Returns true if one of the header strings is not properly formatted indicating a bad moveset file
    bool isHeaderStringMalformated(const char* string, size_t size);

    // Returns true if file exists
    bool fileExists(const char* name);

    // Update a crc32 with new data
    uint32_t crc32_update(uint32_t(&table)[256], uint32_t initial, const void* buf, size_t len);
    // Create a table required for the crc32 generation
    void crc32_generate_table(uint32_t(&table)[256]);
}