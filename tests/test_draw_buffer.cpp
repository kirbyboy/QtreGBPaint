// Headless tests for DrawBuffer::blend_onto: clipping at target edges,
// negative origin and transparent pixels.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "core/color.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"

using PaintCore::ColorIndex;

TEST_CASE("DrawBuffer blends with a negative origin")
{
	ImageBuffer target(4, 4, 2);
	target.fill(0);
	target.clear_dirty_rect();

	DrawBuffer buffer;
	buffer.begin(-2, -1);
	buffer.ensure_fit(1, 1);

	CHECK_EQ(buffer.x(), -2);
	CHECK_EQ(buffer.y(), -1);
	CHECK_EQ(buffer.width(), 4);
	CHECK_EQ(buffer.height(), 3);

	buffer.set_pixel(0, 0, 1); // target (-2,-1): outside
	buffer.set_pixel(1, 0, 2); // target (-1,-1): outside
	buffer.set_pixel(2, 1, 3); // target (0,0)
	buffer.set_pixel(3, 2, 1); // target (1,1)

	buffer.blend_onto(target);

	CHECK_EQ(target.get_pixel(0, 0), 3);
	CHECK_EQ(target.get_pixel(1, 1), 1);
	CHECK_EQ(target.get_pixel(0, 1), 0);
	CHECK_EQ(target.get_pixel(1, 0), 0);
	CHECK_EQ(target.dirty_rect().x, 0);
	CHECK_EQ(target.dirty_rect().y, 0);
	CHECK_EQ(target.dirty_rect().w, 2);
	CHECK_EQ(target.dirty_rect().h, 2);
}

TEST_CASE("DrawBuffer blends with overflow at the target edges")
{
	ImageBuffer target(3, 3, 2);
	target.fill(0);
	target.clear_dirty_rect();

	DrawBuffer buffer;
	buffer.begin(2, 2);
	buffer.ensure_fit(5, 5);

	CHECK_EQ(buffer.width(), 4);
	CHECK_EQ(buffer.height(), 4);

	buffer.set_pixel(0, 0, 1); // target (2,2): inside
	buffer.set_pixel(1, 1, 2); // target (3,3): outside
	buffer.set_pixel(2, 2, 3); // target (4,4): outside
	buffer.set_pixel(3, 3, 1); // target (5,5): outside

	buffer.blend_onto(target);

	CHECK_EQ(target.get_pixel(2, 2), 1);
	CHECK_EQ(target.get_pixel(2, 1), 0);
	CHECK_EQ(target.get_pixel(1, 2), 0);
	CHECK_EQ(target.dirty_rect().x, 2);
	CHECK_EQ(target.dirty_rect().y, 2);
	CHECK_EQ(target.dirty_rect().w, 1);
	CHECK_EQ(target.dirty_rect().h, 1);
}

TEST_CASE("DrawBuffer skips transparent pixels and clears after blending")
{
	ImageBuffer target(3, 3, 2);
	target.fill(0);
	target.clear_dirty_rect();

	DrawBuffer buffer;
	buffer.begin(0, 0);
	buffer.ensure_fit(1, 1);
	buffer.set_pixel(0, 0, PaintCore::TRANSPARENT_INDEX);
	buffer.set_pixel(1, 1, 2);

	buffer.blend_onto(target);

	CHECK_EQ(target.get_pixel(0, 0), 0);
	CHECK_EQ(target.get_pixel(1, 1), 2);

	// blend_onto() clears the buffer: a second blend does not paint again.
	buffer.blend_onto(target);
	CHECK_EQ(buffer.data()[0], PaintCore::TRANSPARENT_INDEX);
	CHECK_EQ(buffer.data()[3], PaintCore::TRANSPARENT_INDEX);
}

TEST_CASE("DrawBuffer inactive blend is a no-op")
{
	ImageBuffer target(2, 2, 1);
	target.fill(0);
	target.clear_dirty_rect();

	DrawBuffer buffer;
	buffer.blend_onto(target);

	CHECK(!target.has_dirty_rect());
	CHECK_EQ(target.get_pixel(0, 0), 0);
	CHECK_EQ(target.get_pixel(1, 1), 0);
}
