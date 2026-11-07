// Headless tests for the .ipx cursor asset loader: index convention, hotspot
// validation and fallback error reporting.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <unistd.h>

#include "core/color.h"
#include "paint/cursor_asset.h"

namespace
{
struct TempIpxFile
{
	std::string path;

	explicit TempIpxFile(const std::string& contents)
	{
		static int counter = 0;
		const std::string name = "qtre_ipx_test_" +
			std::to_string(static_cast<long>(::getpid())) + "_" +
			std::to_string(++counter) + ".ipx";
		path = (std::filesystem::temp_directory_path() / name).string();

		std::ofstream out(path, std::ios::binary);
		out << contents;
	}

	~TempIpxFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

struct LoadResult
{
	bool ok = false;
	PaintCore::CursorImage image;
	std::string error;
};

LoadResult load(const std::string& contents)
{
	const TempIpxFile file(contents);
	LoadResult result;
	result.ok = PaintCore::load_cursor_asset(file.path, result.image,
	                                         &result.error);
	return result;
}

bool same_color(const PaintCore::Color& a, const PaintCore::Color& b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
}

TEST_CASE("Cursor asset loads the 0/1/3/4 convention")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"4 4\n"
		"3\n"
		"0 0 0 0\n"
		"0 1 3 4\n"
		"4 3 0 0\n"
		"0 0 4 3\n");

	CHECK(result.ok);
	CHECK(result.error.empty());
	CHECK_EQ(result.image.width, 4);
	CHECK_EQ(result.image.height, 4);
	CHECK_EQ(result.image.hotspot_x, 1);
	CHECK_EQ(result.image.hotspot_y, 1);
	REQUIRE(result.image.pixels.size() == 16u);

	CHECK(same_color(result.image.pixels[0], {0, 0, 0, 0}));
	CHECK(same_color(result.image.pixels[5], {0, 0, 0, 0}));   // hotspot
	CHECK(same_color(result.image.pixels[6], {0, 0, 0, 255})); // black
	CHECK(same_color(result.image.pixels[7], {255, 255, 255, 255})); // white
}

TEST_CASE("Cursor asset without a hotspot is rejected")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"3\n"
		"0 3\n"
		"4 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("hotspot") != std::string::npos);
}

TEST_CASE("Cursor asset with multiple hotspots is rejected")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"3\n"
		"1 1\n"
		"0 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("hotspot") != std::string::npos);
}

TEST_CASE("Cursor asset rejects unsupported indices")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"3\n"
		"0 2\n"
		"1 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("Unsupported cursor index 2") != std::string::npos);
}

TEST_CASE("Cursor asset requires at least 3 bpp")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"2\n"
		"0 1\n"
		"3 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("3 bpp") != std::string::npos);
}

TEST_CASE("Cursor asset propagates IndexedImage errors")
{
	const LoadResult result = load(
		"IndexedImage Nope\n"
		"2 2\n"
		"3\n"
		"0 0\n"
		"0 0\n");

	CHECK_FALSE(result.ok);
	CHECK_FALSE(result.error.empty());
}
