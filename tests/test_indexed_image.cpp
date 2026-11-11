// Headless tests for IndexedImage: header, modes, strict validation,
// dimensions, bpp, index range and exact value count.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <unistd.h>

#include "core/color.h"
#include "paint/indexed_image.h"

using PaintCore::ColorIndex;

namespace
{
struct TempIdxFile
{
	std::string path;

	explicit TempIdxFile(const std::string& contents)
	{
		static int counter = 0;
		const std::string name = "qtre_idx_test_" +
			std::to_string(static_cast<long>(::getpid())) + "_" +
			std::to_string(++counter) + ".idx";
		path = (std::filesystem::temp_directory_path() / name).string();

		std::ofstream out(path, std::ios::binary);
		out << contents;
	}

	~TempIdxFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

IndexedImage load(const std::string& contents)
{
	const TempIdxFile file(contents);
	return IndexedImage::loadFromFile(file.path);
}
}

TEST_CASE("IndexedImage loads the default Index mode")
{
	const IndexedImage image = load(
		"IndexedImage\n"
		"3 2\n"
		"2\n"
		"0 1 2\n"
		"3 0 1\n");

	CHECK(image.isValid());
	CHECK(image.format() == IndexedFormat::Index);
	CHECK_EQ(image.width(), 3);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.expectedSize(), 6u);
	CHECK_EQ(image.data().size(), 6u);
	CHECK_EQ(image.data()[0], 0);
	CHECK_EQ(image.data()[1], 1);
	CHECK_EQ(image.data()[2], 2);
	CHECK_EQ(image.data()[3], 3);
	CHECK_EQ(image.data()[4], 0);
	CHECK_EQ(image.data()[5], 1);
	CHECK(image.error().empty());
}

TEST_CASE("IndexedImage accepts the explicit Index mode and CRLF")
{
	const IndexedImage image = load(
		"IndexedImage Index\r\n"
		"2 2\r\n"
		"2\r\n"
		"0 1\r\n"
		"2 3\r\n");

	CHECK(image.isValid());
	CHECK_EQ(image.width(), 2);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.data().size(), 4u);
	CHECK_EQ(image.data()[0], 0);
	CHECK_EQ(image.data()[3], 3);
}

TEST_CASE("IndexedImage accepts arbitrary whitespace between values")
{
	const IndexedImage image = load(
		"IndexedImage\n"
		"2 2\n"
		"2\n"
		"0 1 2 3\n");

	CHECK(image.isValid());
	CHECK_EQ(image.data().size(), 4u);
	CHECK_EQ(image.data()[3], 3);
}

TEST_CASE("IndexedImage accepts the full 8bpp index range")
{
	const IndexedImage image = load(
		"IndexedImage\n"
		"2 1\n"
		"8\n"
		"0 255\n");

	CHECK(image.isValid());
	CHECK_EQ(image.bitsPerPixel(), 8);
	CHECK_EQ(image.data()[0], 0);
	CHECK_EQ(image.data()[1], 255);
}

TEST_CASE("IndexedImage round-trips every storage format")
{
	const std::vector<ColorIndex> pixels{0, 1, 2, 3, 1, 0};

	for (IndexedFormat format : {IndexedFormat::Index, IndexedFormat::Binary,
	                             IndexedFormat::Packed,
	                             IndexedFormat::BinaryPacked})
	{
		TempIdxFile file("");
		std::string error;
		REQUIRE(save_indexed_image(file.path, 3, 2, 2, pixels, format, error));
		CHECK(error.empty());

		const IndexedImage image = IndexedImage::loadFromFile(file.path);
		REQUIRE(image.isValid());
		CHECK(image.format() == format);
		CHECK_EQ(image.width(), 3);
		CHECK_EQ(image.height(), 2);
		CHECK_EQ(image.bitsPerPixel(), 2);
		CHECK(image.data() == pixels);
	}
}

TEST_CASE("IndexedImage validates binary payload sizes and Packed digits")
{
	const std::string short_binary = "IndexedImage Binary\n2 2\n2\nabc";
	CHECK(!load(short_binary).isValid());

	const std::string long_binary = "IndexedImage Binary\n2 2\n2\nabcde";
	CHECK(!load(long_binary).isValid());

	const std::string bad_packed = "IndexedImage Packed\n2 1\n2\n0g\n";
	CHECK(!load(bad_packed).isValid());

	const std::string bad_packed_size = "IndexedImage Packed\n3 1\n2\n0\n";
	CHECK(!load(bad_packed_size).isValid());

	const std::string short_packed_binary =
		"IndexedImage BinaryPacked\n3 1\n2\n";
	CHECK(!load(short_packed_binary).isValid());
}

TEST_CASE("IndexedImage reports approximate format sizes")
{
	CHECK_EQ(indexed_image_row_bytes(3, 2), 1u);
	CHECK_EQ(indexed_image_row_bytes(5, 2), 2u);
	CHECK_EQ(indexed_image_data_size(3, 2, 2, IndexedFormat::Binary), 6u);
	CHECK_EQ(indexed_image_data_size(3, 2, 2, IndexedFormat::BinaryPacked), 2u);
	CHECK_EQ(indexed_image_data_size(3, 2, 2, IndexedFormat::Packed), 6u);
	// bpp 2 => max value 3 => one digit; values plus separator/newline.
	CHECK_EQ(indexed_image_data_size(3, 2, 2, IndexedFormat::Index), 12u);
	CHECK_EQ(indexed_image_estimated_file_size(
	             3, 2, 2, IndexedFormat::BinaryPacked),
	         66u);
}

TEST_CASE("IndexedImage rejects malformed headers and payloads")
{
	const IndexedImage empty = load("");
	CHECK(!empty.isValid());
	CHECK(!empty.error().empty());

	const IndexedImage old_magic = load("P2\n2 2\n2\n0 1 2 3\n");
	CHECK(!old_magic.isValid());
	CHECK(!old_magic.error().empty());

	const IndexedImage lower_case = load("indexedimage\n2 2\n2\n0 1 2 3\n");
	CHECK(!lower_case.isValid());
	CHECK(!lower_case.error().empty());

	const IndexedImage binary = load("IndexedImage Binary\n2 2\n2\n0 1 2 3\n");
	CHECK(!binary.isValid());
	CHECK(!binary.error().empty());

	const IndexedImage packed = load("IndexedImage Packed\n2 2\n2\n0 1 2 3\n");
	CHECK(!packed.isValid());
	CHECK(!packed.error().empty());

	const IndexedImage binary_packed =
		load("IndexedImage BinaryPacked\n2 2\n2\n0 1 2 3\n");
	CHECK(!binary_packed.isValid());
	CHECK(!binary_packed.error().empty());

	const IndexedImage unknown = load("IndexedImage Unknown\n2 2\n2\n0 1 2 3\n");
	CHECK(!unknown.isValid());
	CHECK(!unknown.error().empty());

	const IndexedImage extra_token =
		load("IndexedImage Index extra\n2 2\n2\n0 1 2 3\n");
	CHECK(!extra_token.isValid());
	CHECK(!extra_token.error().empty());
}

TEST_CASE("IndexedImage exposes stable error categories")
{
	CHECK_EQ(load("").errorCode(), FileError::TruncatedData);
	CHECK_EQ(load("P2\n2 2\n2\n0 1 2 3\n").errorCode(),
	         FileError::InvalidHeader);
	CHECK_EQ(load("IndexedImage Unknown\n2 2\n2\n0 1 2 3\n").errorCode(),
	         FileError::UnsupportedFormat);
	CHECK_EQ(load("IndexedImage\n0 2\n2\n0 1 2 3\n").errorCode(),
	         FileError::InvalidDimensions);
	CHECK_EQ(load("IndexedImage\n2 2\n0\n0 1 2 3\n").errorCode(),
	         FileError::InvalidBitsPerPixel);
	CHECK_EQ(load("IndexedImage\n2 2\n2\n0 1 2 4\n").errorCode(),
	         FileError::InvalidPixel);
	CHECK_EQ(load("IndexedImage\n2 2\n2\n0 1\n").errorCode(),
	         FileError::TruncatedData);
	CHECK_EQ(load("IndexedImage\n2 2\n2\n0 1 2 3\n9\n").errorCode(),
	         FileError::ExtraData);
}

TEST_CASE("save_indexed_image exposes stable error categories")
{
	std::string error;
	FileError code = FileError::None;
	const std::vector<ColorIndex> pixels = {0, 1, 2, 3};

	CHECK_FALSE(save_indexed_image("", 2, 2, 2, pixels, error, &code));
	CHECK_EQ(code, FileError::NoPath);

	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 0, 2, 2,
	                               pixels, error, &code));
	CHECK_EQ(code, FileError::InvalidDimensions);

	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 9,
	                               pixels, error, &code));
	CHECK_EQ(code, FileError::InvalidBitsPerPixel);

	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 2,
	                               {0, 1, 2}, error, &code));
	CHECK_EQ(code, FileError::InvalidImageData);

	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 2,
	                               {0, 1, 2, 4}, error, &code));
	CHECK_EQ(code, FileError::InvalidPixel);
}

TEST_CASE("IndexedImage rejects invalid dimensions and bpp")
{
	const IndexedImage zero_width = load("IndexedImage\n0 2\n2\n0 1 2 3\n");
	CHECK(!zero_width.isValid());

	const IndexedImage negative_width = load("IndexedImage\n-1 2\n2\n0 1 2 3\n");
	CHECK(!negative_width.isValid());

	const IndexedImage zero_height = load("IndexedImage\n2 0\n2\n0 1 2 3\n");
	CHECK(!zero_height.isValid());

	const IndexedImage non_numeric_width = load("IndexedImage\nx 2\n2\n0 1 2 3\n");
	CHECK(!non_numeric_width.isValid());

	const IndexedImage zero_bpp = load("IndexedImage\n2 2\n0\n0 1 2 3\n");
	CHECK(!zero_bpp.isValid());

	const IndexedImage high_bpp = load("IndexedImage\n2 2\n9\n0 1 2 3\n");
	CHECK(!high_bpp.isValid());

	const IndexedImage non_numeric_bpp = load("IndexedImage\n2 2\nx\n0 1 2 3\n");
	CHECK(!non_numeric_bpp.isValid());
}

TEST_CASE("IndexedImage rejects invalid pixel values")
{
	const IndexedImage out_of_range = load("IndexedImage\n2 2\n2\n0 1 2 4\n");
	CHECK(!out_of_range.isValid());
	CHECK(!out_of_range.error().empty());

	const IndexedImage negative = load("IndexedImage\n2 2\n2\n0 1 2 -1\n");
	CHECK(!negative.isValid());

	const IndexedImage above_byte = load("IndexedImage\n2 2\n2\n0 1 2 256\n");
	CHECK(!above_byte.isValid());

	const IndexedImage non_numeric = load("IndexedImage\n2 2\n2\n0 1 2 value\n");
	CHECK(!non_numeric.isValid());
}

TEST_CASE("IndexedImage requires exactly width*height values")
{
	const IndexedImage insufficient =
		load("IndexedImage\n2 2\n2\n0 1 2\n");
	CHECK(!insufficient.isValid());
	CHECK(!insufficient.error().empty());

	const IndexedImage extra =
		load("IndexedImage\n2 2\n2\n0 1 2 3 4\n");
	CHECK(!extra.isValid());
	CHECK(!extra.error().empty());

	const IndexedImage trailing_garbage =
		load("IndexedImage\n2 2\n2\n0 1 2 3 garbage\n");
	CHECK(!trailing_garbage.isValid());
	CHECK(!trailing_garbage.error().empty());

	const IndexedImage trailing_whitespace =
		load("IndexedImage\n2 2\n2\n0 1 2 3\n\n");
	CHECK(trailing_whitespace.isValid());
}

TEST_CASE("IndexedImage reports a missing file")
{
	const std::string name = "qtre_idx_test_missing_" +
		std::to_string(static_cast<long>(::getpid())) + ".idx";
	const std::string path = (std::filesystem::temp_directory_path() / name).string();
	std::error_code error;
	std::filesystem::remove(path, error);

	const IndexedImage image = IndexedImage::loadFromFile(path);
	CHECK(!image.isValid());
	CHECK(!image.error().empty());
}

TEST_CASE("IndexedImage enforces its model invariants in the constructor")
{
	IndexedImage empty;
	CHECK(!empty.isValid());
	CHECK_EQ(empty.width(), 0);
	CHECK_EQ(empty.height(), 0);
	CHECK_EQ(empty.bitsPerPixel(), 0);
	CHECK_EQ(empty.expectedSize(), 0u);
	CHECK(empty.data().empty());

	const std::vector<ColorIndex> pixels{0, 1, 2, 3};
	const IndexedImage image(2, 2, 2, pixels);
	CHECK(image.isValid());
	CHECK_EQ(image.width(), 2);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.expectedSize(), 4u);
	CHECK_EQ(image.data().size(), 4u);

	CHECK_THROWS_AS(IndexedImage(-1, 2, 2, pixels), std::invalid_argument);
	CHECK_THROWS_AS(IndexedImage(2, -1, 2, pixels), std::invalid_argument);
	CHECK_THROWS_AS(IndexedImage(2, 2, 0, pixels), std::invalid_argument);
	CHECK_THROWS_AS(IndexedImage(2, 2, 9, pixels), std::invalid_argument);
	CHECK_THROWS_AS(IndexedImage(2, 2, 2, {0, 1, 2}), std::invalid_argument);
	CHECK_THROWS_AS(IndexedImage(2, 2, 2, {0, 1, 2, 4}), std::invalid_argument);
}

TEST_CASE("IndexedImage data accessor is read-only")
{
	static_assert(
		std::is_same<decltype(std::declval<const IndexedImage&>().data()),
					 const std::vector<ColorIndex>&>::value,
		"IndexedImage::data() must expose read-only storage");
}

TEST_CASE("save_indexed_image round-trips Index mode")
{
	static int counter = 0;
	const std::string path = (std::filesystem::temp_directory_path() /
		("qtre_idx_write_" + std::to_string(static_cast<long>(::getpid())) +
		 "_" + std::to_string(++counter) + ".idx")).string();

	const std::vector<ColorIndex> pixels{0, 1, 2, 3};
	std::string error;
	REQUIRE(save_indexed_image(path, 2, 2, 2, pixels, error));
	CHECK(error.empty());

	const IndexedImage loaded = IndexedImage::loadFromFile(path);
	REQUIRE(loaded.isValid());
	CHECK_EQ(loaded.width(), 2);
	CHECK_EQ(loaded.height(), 2);
	CHECK_EQ(loaded.bitsPerPixel(), 2);
	CHECK(loaded.data() == pixels);

	std::error_code remove_error;
	std::filesystem::remove(path, remove_error);
}

TEST_CASE("save_indexed_image rejects invalid data")
{
	std::string error;
	const std::vector<ColorIndex> pixels{0, 1, 2, 3};

	CHECK_FALSE(save_indexed_image("", 2, 2, 2, pixels, error));
	CHECK_FALSE(error.empty());

	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 0, 2, 2, pixels, error));
	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, -1, 2, pixels, error));
	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 0, pixels, error));
	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 9, pixels, error));
	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 2, {0, 1, 2}, error));
	CHECK_FALSE(save_indexed_image("/tmp/qtre_invalid.idx", 2, 2, 2, {0, 1, 2, 4}, error));
}
