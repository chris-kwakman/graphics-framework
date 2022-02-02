#include <gtest/gtest.h>

#include <engine/Serialisation/compress.h>
#include <algorithm>

template<typename T>
void test_compressing_type(std::vector<T> _values)
{
	auto compressed = Engine::Serialisation::compress_data_vector(_values);
	auto decompressed = Engine::Serialisation::decompress_data_vector<T>(compressed, _values.size());
	EXPECT_EQ(_values.size(), decompressed.size());
	EXPECT_TRUE(
		std::equal(
			_values.begin(), _values.end(),
			decompressed.begin()
		)
	);
	if (std::is_same<T, bool>::value)
	{
		EXPECT_LE(_values.size(), compressed.size() * sizeof(decltype(compressed)::value_type) * 8);
	}
	else
	{
		EXPECT_LE(_values.size(), compressed.size() * sizeof(decltype(compressed)::value_type) / sizeof(T));
	}
}

TEST(Compression, Mask)
{
	using namespace Engine::Serialisation;
	auto test = [](size_t _idx, size_t _type_bit_size)
	{
		size_t mask = get_compressed_type_entry_mask(_idx, _type_bit_size);
		EXPECT_EQ(mask, ((size_t(1u) << _type_bit_size)-1) << (_type_bit_size * _idx));
	};

	test(0, 1);
	test(0, 8);
	test(1, 8);
	test(2, 8);
	test(3, 8);
	test(0, 16);
	test(1, 16);
	test(0, 32);
	test(1, 32);
	test(0, 64);
}

TEST(Compression, SetEntry)
{
	using namespace Engine::Serialisation;
	auto test = [](compressed_type _compressed_element, size_t _idx, size_t _type_bit_size, size_t _entry, compressed_type _expected)
	{
		set_compressed_type_entry(_compressed_element, _entry, _idx, _type_bit_size);
		EXPECT_EQ(_compressed_element, _expected);
	};

	test(0b0, 0, 1, 0b1, 0b01);
	test(0b0, 1, 1, 0b1, 0b10);
	test(0b0, 0, 2, 0b01, 0b00'01);
	test(0b0, 1, 2, 0b01, 0b01'00);
	test(0x00'00, 1, 8, 0xFF, 0xFF'00);
}

TEST(Compression, GetEntry)
{
	using namespace Engine::Serialisation;
	auto test = [](compressed_type _compressed_element, size_t _idx, size_t _type_bit_size, size_t _expected)
	{
		EXPECT_EQ(get_compressed_type_entry(_compressed_element, _idx, _type_bit_size), _expected);
	};

	test(0b0'1, 0, 1, 0b1);
	test(0b0'1, 1, 1, 0b0);
	test(0b10'01, 0, 2, 0b01);
	test(0b10'01, 1, 2, 0b10);
	test(0xFF'AA, 0, 8, 0xAA);
	test(0xFF'AA, 1, 8, 0xFF);
}

TEST(Compression, CompressBigTypes)
{
	test_compressing_type(std::vector<bool>{true, false, true});
	test_compressing_type(std::vector<uint8_t>{1, 2, 3});
	test_compressing_type(std::vector<uint16_t>{1, 2, 3});
	test_compressing_type(std::vector<uint32_t>{1, 2, 3});
	test_compressing_type(std::vector<uint64_t>{1, 2, 3});
	test_compressing_type(std::vector<float>{0.0f, -1.0f, 1.0f});
}