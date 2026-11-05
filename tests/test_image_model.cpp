// Tests headless del modelo ImageBuffer/Palette: acceso, validación de bpp
// y preservación de metadatos. Ejecutar con `make -C tests` o `make test` desde la raíz.

#include "doctest/doctest.h"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/color.h"
#include "paint/image_buffer.h"
#include "paint/palette.h"

using PaintCore::Color;
using PaintCore::ColorIndex;

namespace
{
bool color_eq(const Color& color, int r, int g, int b, int a)
{
	return color.r == r && color.g == g && color.b == b && color.a == a;
}

#define CHECK_COLOR(actual, r, g, b, a) \
	CHECK(color_eq((actual), (r), (g), (b), (a)))

template <typename Fn>
bool throws_invalid_argument(Fn&& fn)
{
	try
	{
		fn();
	}
	catch (const std::invalid_argument&)
	{
		return true;
	}
	catch (...)
	{
		return false;
	}
	return false;
}

TEST_CASE("test_image_buffer_accessors")
{
	ImageBuffer image(4, 3, 2);

	CHECK_EQ(image.width(), 4);
	CHECK_EQ(image.height(), 3);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.data().size(), 12u);
	CHECK(image.has_dirty_rect());
}

TEST_CASE("test_image_buffer_rejects_invalid_model")
{
	CHECK(throws_invalid_argument([] { ImageBuffer(-1, 2, 2); }));
	CHECK(throws_invalid_argument([] { ImageBuffer(2, -1, 2); }));
	CHECK(throws_invalid_argument([] { ImageBuffer(2, 2, 0); }));
	CHECK(throws_invalid_argument([] { ImageBuffer(2, 2, 9); }));
	CHECK(throws_invalid_argument([] { ImageBuffer(2, 2, 32); }));
	CHECK(throws_invalid_argument([] {
		ImageBuffer image(2, 2, 2);
		image.resize(-1, 2);
	}));
	CHECK(throws_invalid_argument([] {
		ImageBuffer image(2, 2, 2);
		image.resize(2, -1);
	}));
}

TEST_CASE("test_image_buffer_default_state")
{
	ImageBuffer image;
	CHECK_EQ(image.width(), 0);
	CHECK_EQ(image.height(), 0);
	CHECK_EQ(image.bitsPerPixel(), 8);
	CHECK(image.data().empty());
}

TEST_CASE("test_sub_buffer_preserves_metadata")
{
	ImageBuffer image(4, 3, 3);
	image.fill(0);
	image.set_pixel(1, 1, 5);
	image.set_pixel(2, 1, 6);

	const ImageBuffer sub = image.sub_buffer(1, 1, 2, 2);
	CHECK_EQ(sub.width(), 2);
	CHECK_EQ(sub.height(), 2);
	CHECK_EQ(sub.bitsPerPixel(), 3);
	CHECK_EQ(sub.get_pixel(0, 0), 5);
	CHECK_EQ(sub.get_pixel(1, 0), 6);
	CHECK_EQ(sub.get_pixel(0, 1), 0);
}

TEST_CASE("test_copy_from_requires_matching_bpp")
{
	ImageBuffer two_bpp(2, 2, 2);
	ImageBuffer three_bpp(2, 2, 3);
	three_bpp.fill(7);

	CHECK(throws_invalid_argument([&] { two_bpp.copy_from(three_bpp, 0, 0); }));

	ImageBuffer same_bpp(2, 2, 3);
	same_bpp.copy_from(three_bpp, 0, 0);
	CHECK_EQ(same_bpp.get_pixel(0, 0), 7);
	CHECK_EQ(same_bpp.get_pixel(1, 1), 7);
}

TEST_CASE("test_resize_preserves_content_and_marks_dirty")
{
	ImageBuffer image(3, 2, 2);
	image.set_pixel(0, 0, 1);
	image.set_pixel(2, 1, 2);
	image.clear_dirty_rect();

	image.resize(5, 4);
	CHECK_EQ(image.width(), 5);
	CHECK_EQ(image.height(), 4);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.data().size(), 20u);
	CHECK_EQ(image.get_pixel(0, 0), 1);
	CHECK_EQ(image.get_pixel(2, 1), 2);
	CHECK_EQ(image.get_pixel(4, 3), 0);
	CHECK(image.has_dirty_rect());
	CHECK_EQ(image.dirty_rect().x, 0);
	CHECK_EQ(image.dirty_rect().y, 0);
	CHECK_EQ(image.dirty_rect().w, 5);
	CHECK_EQ(image.dirty_rect().h, 4);
}

TEST_CASE("test_data_accessor_is_read_only")
{
	static_assert(
		std::is_same<decltype(std::declval<const ImageBuffer&>().data()),
					 const std::vector<ColorIndex>&>::value,
		"ImageBuffer::data() must expose read-only storage");
}

TEST_CASE("test_palette_valid_sizes")
{
	for (int bpp = 1; bpp <= 8; ++bpp)
	{
		Palette palette(static_cast<std::size_t>(bpp));
		CHECK_EQ(palette.bitsPerPixel(), static_cast<std::size_t>(bpp));
		CHECK_EQ(palette.size(), static_cast<std::size_t>(1) << bpp);
	}
}

TEST_CASE("test_palette_rejects_invalid_bpp")
{
	CHECK(throws_invalid_argument([] { Palette(0); }));
	CHECK(throws_invalid_argument([] { Palette(9); }));
	CHECK(throws_invalid_argument([] { Palette(32); }));
	CHECK(throws_invalid_argument([] { Palette(64); }));
	CHECK(throws_invalid_argument([] { Palette::grayscale(0); }));
	CHECK(throws_invalid_argument([] { Palette::grayscale(9); }));
}

TEST_CASE("test_palette_dmg_and_grayscale")
{
	const Palette dmg = Palette::dmg();
	CHECK_EQ(dmg.bitsPerPixel(), 2u);
	CHECK_EQ(dmg.size(), 4u);
	CHECK_COLOR(dmg.getColor(0), 0xE0, 0xF8, 0xD0, 0xFF);
	CHECK_COLOR(dmg.getColor(3), 0x08, 0x18, 0x20, 0xFF);

	const Palette one_bit = Palette::grayscale(1);
	CHECK_EQ(one_bit.size(), 2u);
	CHECK_COLOR(one_bit.getColor(0), 0, 0, 0, 0xFF);
	CHECK_COLOR(one_bit.getColor(1), 255, 255, 255, 0xFF);

	const Palette eight_bit = Palette::grayscale(8);
	CHECK_EQ(eight_bit.size(), 256u);
	CHECK_COLOR(eight_bit.getColor(0), 0, 0, 0, 0xFF);
	CHECK_COLOR(eight_bit.getColor(255), 255, 255, 255, 0xFF);
}
}
