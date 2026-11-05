#include "doctest/doctest.h"

#include "paint/image_buffer.h"

static bool rect_eq(const PaintCore::Rect& r, int x, int y, int w, int h)
{
	return r.x == x && r.y == y && r.w == w && r.h == h;
}

TEST_CASE("ImageBuffer dirty rect")
{
	ImageBuffer img(10, 10, 2);
	CHECK(img.has_dirty_rect());
	CHECK(rect_eq(img.dirty_rect(), 0, 0, 10, 10));

	img.clear_dirty_rect();
	CHECK(!img.has_dirty_rect());

	img.set_pixel(2, 3, 1);
	CHECK(rect_eq(img.dirty_rect(), 2, 3, 1, 1));

	img.set_pixel(5, 1, 2);
	CHECK(rect_eq(img.dirty_rect(), 2, 1, 4, 3));

	img.set_pixel(-1, 0, 1);
	img.set_pixel(10, 0, 1);
	CHECK(rect_eq(img.dirty_rect(), 2, 1, 4, 3));

	img.clear_dirty_rect();
	ImageBuffer src(3, 2, 2);
	src.fill(1);
	img.copy_from(src, 8, 9);
	CHECK(rect_eq(img.dirty_rect(), 8, 9, 2, 1));

	img.clear_dirty_rect();
	img.fill(0);
	CHECK(rect_eq(img.dirty_rect(), 0, 0, 10, 10));

	img.clear_dirty_rect();
	img.resize(4, 7);
	CHECK(rect_eq(img.dirty_rect(), 0, 0, 4, 7));

	img.clear_dirty_rect();
	img.mark_all_dirty();
	CHECK(rect_eq(img.dirty_rect(), 0, 0, 4, 7));
}
