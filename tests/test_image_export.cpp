#include "doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <zlib.h>

#include "paint/image_buffer.h"
#include "paint/image_export.h"
#include "paint/palette.h"

using PaintCore::ColorIndex;

namespace
{
class TempDir
{
public:
	TempDir()
	{
		const auto stamp =
			std::chrono::steady_clock::now().time_since_epoch().count();
		m_path = std::filesystem::temp_directory_path() /
		         ("qtregbpaint_export_" + std::to_string(stamp));
		std::filesystem::create_directories(m_path);
	}

	~TempDir()
	{
		std::error_code error;
		std::filesystem::remove_all(m_path, error);
	}

	std::string file(const std::string& name) const
	{
		return (m_path / name).string();
	}

private:
	std::filesystem::path m_path;
};

std::vector<std::uint8_t> read_file(const std::string& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>());
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(bytes[offset]) |
	       static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
	return static_cast<std::uint32_t>(bytes[offset]) |
	       (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
	       (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
	       (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint32_t read_u32_be(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset)
{
	return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
	       (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
	       (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
	       static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::string chunk_type(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset)
{
	return std::string(reinterpret_cast<const char*>(bytes.data() + offset), 4);
}

struct PngChunk
{
	std::string type;
	std::vector<std::uint8_t> data;
};

bool parse_png(const std::vector<std::uint8_t>& bytes,
               std::vector<PngChunk>& chunks)
{
	static const std::uint8_t signature[8] = {
		0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	if (bytes.size() < 8 ||
	    !std::equal(std::begin(signature), std::end(signature), bytes.begin()))
	{
		return false;
	}

	chunks.clear();
	std::size_t offset = 8;
	while (offset + 12 <= bytes.size())
	{
		const std::uint32_t length = read_u32_be(bytes, offset);
		if (length > bytes.size() - offset - 12)
		{
			return false;
		}
		PngChunk chunk;
		chunk.type = chunk_type(bytes, offset + 4);
		chunk.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 8),
		                  bytes.begin() + static_cast<std::ptrdiff_t>(
		                      offset + 8 + length));
		const std::uint32_t stored_crc = read_u32_be(
			bytes, offset + 8 + length);

		uLong crc = crc32(0L, Z_NULL, 0);
		crc = crc32(crc, reinterpret_cast<const Bytef*>(chunk.type.data()), 4);
		if (!chunk.data.empty())
		{
			crc = crc32(crc, chunk.data.data(),
			            static_cast<uInt>(chunk.data.size()));
		}
		if (static_cast<std::uint32_t>(crc) != stored_crc)
		{
			return false;
		}

		chunks.push_back(std::move(chunk));
		offset += 12 + length;
	}
	return offset == bytes.size();
}
}

TEST_CASE("BMP export writes a 24-bit bottom-up BGR image")
{
	TempDir dir;
	ImageBuffer image(2, 2, 2);
	image.set_pixel(0, 0, 0);
	image.set_pixel(1, 0, 1);
	image.set_pixel(0, 1, 2);
	image.set_pixel(1, 1, 3);
	Palette palette = Palette::dmg();
	palette.set_transparent_index(2); // BMP must use the stored color

	const std::string path = dir.file("image.bmp");
	std::string error;
	REQUIRE(export_bmp(path, image, palette, error));
	CHECK(error.empty());

	const std::vector<std::uint8_t> bytes = read_file(path);
	REQUIRE_EQ(bytes.size(), 70u);
	CHECK_EQ(bytes[0], 'B');
	CHECK_EQ(bytes[1], 'M');
	CHECK_EQ(read_u32(bytes, 2), 70u);
	CHECK_EQ(read_u32(bytes, 10), 54u);

	// BITMAPINFOHEADER
	CHECK_EQ(read_u32(bytes, 14), 40u);
	CHECK_EQ(static_cast<std::int32_t>(read_u32(bytes, 18)), 2);
	CHECK_EQ(static_cast<std::int32_t>(read_u32(bytes, 22)), 2);
	CHECK_EQ(read_u16(bytes, 26), 1u);
	CHECK_EQ(read_u16(bytes, 28), 24u);
	CHECK_EQ(read_u32(bytes, 30), 0u);
	CHECK_EQ(read_u32(bytes, 34), 16u);

	// Bottom row first (y == 1): indices 2 and 3.
	const PaintCore::Color color2 = palette.getStoredColor(2);
	const PaintCore::Color color3 = palette.getStoredColor(3);
	CHECK_EQ(bytes[54], color2.b);
	CHECK_EQ(bytes[55], color2.g);
	CHECK_EQ(bytes[56], color2.r);
	CHECK_EQ(bytes[57], color3.b);
	CHECK_EQ(bytes[58], color3.g);
	CHECK_EQ(bytes[59], color3.r);
	CHECK_EQ(bytes[60], 0u);
	CHECK_EQ(bytes[61], 0u);

	// Top row (y == 0): indices 0 and 1.
	const PaintCore::Color color0 = palette.getStoredColor(0);
	const PaintCore::Color color1 = palette.getStoredColor(1);
	CHECK_EQ(bytes[62], color0.b);
	CHECK_EQ(bytes[63], color0.g);
	CHECK_EQ(bytes[64], color0.r);
	CHECK_EQ(bytes[65], color1.b);
	CHECK_EQ(bytes[66], color1.g);
	CHECK_EQ(bytes[67], color1.r);
	CHECK_EQ(bytes[68], 0u);
	CHECK_EQ(bytes[69], 0u);
}

TEST_CASE("PNG export writes an indexed image with palette transparency")
{
	TempDir dir;
	ImageBuffer image(2, 2, 2);
	image.set_pixel(0, 0, 0);
	image.set_pixel(1, 0, 1);
	image.set_pixel(0, 1, 2);
	image.set_pixel(1, 1, 3);
	Palette palette = Palette::dmg();
	palette.set_transparent_index(2);

	const std::string path = dir.file("image.png");
	std::string error;
	REQUIRE(export_png(path, image, palette, error));
	CHECK(error.empty());

	const std::vector<std::uint8_t> bytes = read_file(path);
	std::vector<PngChunk> chunks;
	REQUIRE(parse_png(bytes, chunks));
	REQUIRE_EQ(chunks.size(), 5u);
	CHECK_EQ(chunks[0].type, "IHDR");
	CHECK_EQ(chunks[1].type, "PLTE");
	CHECK_EQ(chunks[2].type, "tRNS");
	CHECK_EQ(chunks[3].type, "IDAT");
	CHECK_EQ(chunks[4].type, "IEND");

	REQUIRE_EQ(chunks[0].data.size(), 13u);
	CHECK_EQ(read_u32_be(chunks[0].data, 0), 2u);
	CHECK_EQ(read_u32_be(chunks[0].data, 4), 2u);
	CHECK_EQ(chunks[0].data[8], 8u); // bit depth
	CHECK_EQ(chunks[0].data[9], 3u); // indexed color
	CHECK_EQ(chunks[0].data[10], 0u);
	CHECK_EQ(chunks[0].data[11], 0u);
	CHECK_EQ(chunks[0].data[12], 0u);

	REQUIRE_EQ(chunks[1].data.size(), 12u);
	for (int index = 0; index < 4; ++index)
	{
		const PaintCore::Color color = palette.getStoredColor(
			static_cast<ColorIndex>(index));
		CHECK_EQ(chunks[1].data[index * 3], color.r);
		CHECK_EQ(chunks[1].data[index * 3 + 1], color.g);
		CHECK_EQ(chunks[1].data[index * 3 + 2], color.b);
	}

	REQUIRE_EQ(chunks[2].data.size(), 3u);
	CHECK_EQ(chunks[2].data[0], 255u);
	CHECK_EQ(chunks[2].data[1], 255u);
	CHECK_EQ(chunks[2].data[2], 0u);

	const std::vector<std::uint8_t>& idat = chunks[3].data;
	const std::size_t expected_raw = 2u * (1u + 2u);
	std::vector<std::uint8_t> raw(expected_raw);
	uLongf raw_size = static_cast<uLongf>(raw.size());
	REQUIRE_EQ(uncompress(raw.data(), &raw_size, idat.data(),
	                      static_cast<uLong>(idat.size())),
	           Z_OK);
	CHECK_EQ(raw_size, expected_raw);
	CHECK_EQ(raw[0], 0u);
	CHECK_EQ(raw[1], 0u);
	CHECK_EQ(raw[2], 1u);
	CHECK_EQ(raw[3], 0u);
	CHECK_EQ(raw[4], 2u);
	CHECK_EQ(raw[5], 3u);
}

TEST_CASE("Image export rejects invalid input")
{
	TempDir dir;
	ImageBuffer image(1, 1, 1);
	image.set_pixel(0, 0, 2); // out of range for a 1bpp/2-color palette
	const Palette palette = Palette::grayscale(1);
	std::string error;

	CHECK_FALSE(export_bmp("", image, palette, error));
	CHECK(error.empty() == false);
	CHECK_FALSE(export_png(dir.file("bad.png"), image, palette, error));
	CHECK(error.empty() == false);

	const Palette wrong_size = Palette::grayscale(2);
	CHECK_FALSE(export_bmp(dir.file("wrong.bmp"), image, wrong_size, error));
	CHECK(error.empty() == false);
}
