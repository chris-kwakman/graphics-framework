#include <vector>
#include <cstdint>
#include <typeinfo>

namespace Engine {
namespace Serialisation
{

	typedef uint64_t compressed_type;

	uint64_t get_compressed_type_entry_mask(size_t _index, size_t _type_bits)
	{
		return ((compressed_type(1) << _type_bits) - 1) << (_index * _type_bits);
	}

	void set_compressed_type_entry(compressed_type & _element, uint64_t _entry, size_t _index, size_t _type_bits)
	{
		uint64_t const mask = get_compressed_type_entry_mask(_index, _type_bits);
		_element = (_element & ~mask) | ((_entry << (_index * _type_bits)) & mask);
	}

	uint64_t get_compressed_type_entry(compressed_type _element, size_t _index, size_t _type_bits)
	{
		return (_element & get_compressed_type_entry_mask(_index, _type_bits)) >> (_index * _type_bits);
	}

	template<typename T>
	std::vector<compressed_type> compress_data_vector(std::vector<T> const& _vector);
	template<typename T>
	std::vector<T> decompress_data_vector(std::vector<compressed_type> const& _vector, unsigned int const _enforce_total_entries = 0);

	template<typename T>
	std::vector<compressed_type> compress_data_vector(std::vector<T> const& _vector)
	{
		if constexpr (sizeof(compressed_type) == sizeof(T))
			return _vector;

		constexpr unsigned int const BIT_MULT = std::is_same<T, bool>::value ? 1 : 8;

		// Compress input vector entries into compressed type element
		unsigned int const ENTRIES_PER_ELEMENT = sizeof(compressed_type) / sizeof(T);
		unsigned int const total_elements = (unsigned int)std::ceil(float(_vector.size()) / float(ENTRIES_PER_ELEMENT));

		std::vector<compressed_type> compressed_vector(total_elements, 0);
		for (size_t i = 0; i < _vector.size(); i++)
		{
			T const element = _vector[i];
			set_compressed_type_entry(compressed_vector[std::floor((float)i / (float)ENTRIES_PER_ELEMENT)], *(uint64_t*)(&element), i % ENTRIES_PER_ELEMENT, sizeof(T) * BIT_MULT);
		}

		return compressed_vector;
	}

	template<typename T>
	std::vector<T> decompress_data_vector(std::vector<compressed_type> const& _vector, unsigned int const _enforce_total_entries)
	{
		if constexpr (sizeof(compressed_type) == sizeof(T))
			return _vector;

		constexpr unsigned int const BIT_MULT = std::is_same<T, bool>::value ? 1 : 8;

		unsigned int ENTRIES_PER_ELEMENT = sizeof(compressed_type) / sizeof(T);
		unsigned int const total_entries = (_enforce_total_entries == 0) ? _vector.size() * ENTRIES_PER_ELEMENT : _enforce_total_entries;

		std::vector<T> decompressed_vector(total_entries, (T)0);
		for (size_t i = 0; i < decompressed_vector.size(); i++)
		{
			unsigned int const element_entry_index = i % ENTRIES_PER_ELEMENT;
			uint64_t const result = get_compressed_type_entry(_vector[std::floor((float)i / (float)ENTRIES_PER_ELEMENT)], i % ENTRIES_PER_ELEMENT, sizeof(T) * BIT_MULT) & ((uint64_t(1) << sizeof(T) * BIT_MULT) - 1);
			decompressed_vector[i] = *(T*)&result;
		}

		return decompressed_vector;
	}
}
}