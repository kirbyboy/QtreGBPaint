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

LoadResult load(const std::string& contents, bool black_as_dark_gray = false)
{
	const TempIpxFile file(contents);
	LoadResult result;
	result.ok = PaintCore::load_cursor_asset(file.path, result.image,
	                                         &result.error,
	                                         black_as_dark_gray);
	return result;
}

bool same_color(const PaintCore::Color& a, const PaintCore::Color& b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
}

TEST_CASE("Cursor asset loads the 0/1/2/3 convention")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"4 4\n"
		"2\n"
		"0 0 0 0\n"
		"0 1 2 3\n"
		"3 2 0 0\n"
		"0 0 3 2\n");

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
		"2\n"
		"0 2\n"
		"3 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("hotspot") != std::string::npos);
}

TEST_CASE("Cursor asset with multiple hotspots is rejected")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"2\n"
		"1 1\n"
		"0 0\n");

	CHECK_FALSE(result.ok);
	CHECK(result.error.find("hotspot") != std::string::npos);
}

TEST_CASE("Brush cursor mode renders index 2 as dark gray")
{
	const LoadResult result = load(
		"IndexedImage Index\n"
		"2 2\n"
		"2\n"
		"0 2\n"
		"1 3\n",
		true);

	CHECK(result.ok);
	CHECK(result.error.empty());
	CHECK_EQ(result.image.hotspot_x, 0);
	CHECK_EQ(result.image.hotspot_y, 1);
	REQUIRE(result.image.pixels.size() == 4u);
	CHECK(same_color(result.image.pixels[0], {0, 0, 0, 0}));
	CHECK(same_color(result.image.pixels[1], {96, 96, 96, 255})); // index 2
	CHECK(same_color(result.image.pixels[2], {0, 0, 0, 0}));      // hotspot
	CHECK(same_color(result.image.pixels[3], {255, 255, 255, 255})); // index 3
}

TEST_CASE("Cursor asset requires exactly 2 bpp")
{
	const LoadResult one_bpp = load(
		"IndexedImage Index\n"
		"2 2\n"
		"1\n"
		"0 1\n"
		"0 0\n");
	CHECK_FALSE(one_bpp.ok);
	CHECK(one_bpp.error.find("2 bpp") != std::string::npos);

	const LoadResult three_bpp = load(
		"IndexedImage Index\n"
		"2 2\n"
		"3\n"
		"0 1\n"
		"0 0\n");
	CHECK_FALSE(three_bpp.ok);
	CHECK(three_bpp.error.find("2 bpp") != std::string::npos);
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

TEST_CASE("Brush cursor asset is a low-visibility alternating crosshair")
{
	const std::filesystem::path asset =
		std::filesystem::path(__FILE__).parent_path().parent_path() /
		"assets" / "cursors" / "slim_cross.ipx";
	PaintCore::CursorImage image;
	std::string error;

	REQUIRE(PaintCore::load_cursor_asset(asset.string(), image, &error,
	                                      true));
	CHECK(error.empty());
	CHECK_EQ(image.width, 17);
	CHECK_EQ(image.height, 17);
	CHECK_EQ(image.hotspot_x, 8);
	CHECK_EQ(image.hotspot_y, 8);
	REQUIRE(image.pixels.size() == 17u * 17u);

	const auto pixel = [&image](int x, int y)
	{
		return image.pixels[static_cast<std::size_t>(y * image.width + x)];
	};

	CHECK(same_color(pixel(8, 8), {0, 0, 0, 0}));
	CHECK(same_color(pixel(9, 8), {255, 255, 255, 255}));
	CHECK(same_color(pixel(10, 8), {96, 96, 96, 255}));
	CHECK(same_color(pixel(8, 9), {255, 255, 255, 255}));
	CHECK(same_color(pixel(8, 10), {96, 96, 96, 255}));
	CHECK(same_color(pixel(0, 8), {96, 96, 96, 255}));
	CHECK(same_color(pixel(0, 0), {0, 0, 0, 0}));

	// The same file loaded as a normal cursor keeps index 2 black.
	PaintCore::CursorImage black_image;
	std::string black_error;
	REQUIRE(PaintCore::load_cursor_asset(asset.string(), black_image,
	                                     &black_error));
	CHECK(black_error.empty());
	REQUIRE(black_image.pixels.size() == 17u * 17u);
	CHECK(same_color(black_image.pixels[8 * 17 + 10], {0, 0, 0, 255}));
	CHECK(same_color(black_image.pixels[8 * 17 + 9],
	                 {255, 255, 255, 255}));
}

TEST_CASE("Fill cursor asset has its hotspot at the paint tip")
{
	const std::filesystem::path asset =
		std::filesystem::path(__FILE__).parent_path().parent_path() /
		"assets" / "cursors" / "fill.ipx";
	PaintCore::CursorImage image;
	std::string error;

	REQUIRE(PaintCore::load_cursor_asset(asset.string(), image, &error));
	CHECK(error.empty());
	CHECK_EQ(image.width, 19);
	CHECK_EQ(image.height, 18);
	CHECK_EQ(image.hotspot_x, 2);
	CHECK_EQ(image.hotspot_y, 16);
	REQUIRE(image.pixels.size() == 19u * 18u);

	const auto pixel = [&image](int x, int y)
	{
		return image.pixels[static_cast<std::size_t>(y * image.width + x)];
	};

	// The hotspot is transparent; the outline is black and the highlight white.
	CHECK(same_color(pixel(2, 16), {0, 0, 0, 0}));
	CHECK(same_color(pixel(9, 1), {0, 0, 0, 255}));
	CHECK(same_color(pixel(9, 0), {255, 255, 255, 255}));
	CHECK(same_color(pixel(0, 0), {0, 0, 0, 0}));
}

TEST_CASE("Invisible eraser cursor is fully transparent")
{
	const PaintCore::CursorImage image = PaintCore::make_invisible_cursor();
	CHECK_EQ(image.width, 1);
	CHECK_EQ(image.height, 1);
	CHECK_EQ(image.hotspot_x, 0);
	CHECK_EQ(image.hotspot_y, 0);
	REQUIRE(image.pixels.size() == 1u);
	CHECK(same_color(image.pixels[0], {0, 0, 0, 0}));
}

TEST_CASE("Pencil cursor asset has its hotspot at the tip")
{
	const std::filesystem::path asset =
		std::filesystem::path(__FILE__).parent_path().parent_path() /
		"assets" / "cursors" / "pencil.ipx";
	PaintCore::CursorImage image;
	std::string error;

	REQUIRE(PaintCore::load_cursor_asset(asset.string(), image, &error));
	CHECK(error.empty());
	CHECK_EQ(image.width, 18);
	CHECK_EQ(image.height, 18);
	CHECK_EQ(image.hotspot_x, 4);
	CHECK_EQ(image.hotspot_y, 16);
	REQUIRE(image.pixels.size() == 18u * 18u);
	// The hotspot convention renders that pixel transparent.
	CHECK(same_color(image.pixels[16 * 18 + 4], {0, 0, 0, 0}));
	// The pencil body uses the new 2 bpp black index.
	CHECK(same_color(image.pixels[2 * 18 + 8], {0, 0, 0, 255}));
}
