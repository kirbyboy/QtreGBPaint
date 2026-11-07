// Headless tests for UndoHistory, ImageBuffer and Canvas.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "core/color.h"
#include "core/rect.h"
#include "core/vec2.h"
#include "paint/canvas.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/undo_history.h"
#include "tools/base_tool.h"

using PaintCore::ColorIndex;
using PaintCore::Rect;

namespace
{
bool rect_eq(const Rect& a, const Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

UndoStep single_pixel_step(int x, int y, ColorIndex before, ColorIndex after)
{
	UndoStep step;
	step.patches.push_back(PixelPatch{Rect{x, y, 1, 1}, {before}, {after}});
	return step;
}

void paint_pixel(Canvas& canvas, int x, int y, ColorIndex value)
{
	DrawBuffer buffer;
	buffer.begin(x, y);
	buffer.set_pixel(0, 0, value);
	canvas.commit_draw_buffer(buffer);
}

TEST_CASE("test_region_read_write")
{
	ImageBuffer image(3, 2, 2);
	image.fill(0);
	image.set_pixel(0, 0, 1);
	image.set_pixel(1, 0, 2);
	image.set_pixel(2, 0, 3);
	image.set_pixel(0, 1, 4);
	image.set_pixel(1, 1, 5);
	image.set_pixel(2, 1, 6);

	const std::vector<ColorIndex> region = image.read_region(Rect{0, 0, 3, 2});
	CHECK_EQ(region.size(), 6u);
	CHECK_EQ(region[0], 1);
	CHECK_EQ(region[1], 2);
	CHECK_EQ(region[2], 3);
	CHECK_EQ(region[3], 4);
	CHECK_EQ(region[4], 5);
	CHECK_EQ(region[5], 6);

	CHECK(image.read_region(Rect{-1, 0, 1, 1}).empty());
	CHECK(image.read_region(Rect{0, 0, 4, 1}).empty());
	CHECK(image.read_region(Rect{0, 0, 0, 1}).empty());

	image.clear_dirty_rect();
	const ColorIndex replacement[2] = {9, 8};
	image.write_region(Rect{1, 1, 2, 1}, replacement);
	CHECK_EQ(image.get_pixel(1, 1), 9);
	CHECK_EQ(image.get_pixel(2, 1), 8);
	CHECK(image.has_dirty_rect());
	CHECK(rect_eq(image.dirty_rect(), Rect{1, 1, 2, 1}));

	image.clear_dirty_rect();
	image.write_region(Rect{2, 0, 2, 1}, replacement);
	CHECK(!image.has_dirty_rect());
	CHECK_EQ(image.get_pixel(2, 0), 3);

	image.clear_dirty_rect();
	image.write_region(Rect{0, 0, 1, 1}, nullptr);
	CHECK(!image.has_dirty_rect());
}

TEST_CASE("test_push_undo_redo")
{
	ImageBuffer image(4, 4, 2);
	image.fill(0);

	UndoHistory history;
	UndoStep step = single_pixel_step(1, 1, 0, 5);
	const std::vector<ColorIndex> after = {5};
	image.write_region(Rect{1, 1, 1, 1}, after.data());
	image.clear_dirty_rect();

	history.push(std::move(step));
	CHECK(history.can_undo());
	CHECK(!history.can_redo());
	CHECK_EQ(history.step_count(), 1u);
	CHECK_EQ(history.bytes(), 2u);

	CHECK(history.undo(image));
	CHECK(!history.can_undo());
	CHECK(history.can_redo());
	CHECK_EQ(history.bytes(), 2u);
	CHECK_EQ(image.get_pixel(1, 1), 0);
	CHECK(image.has_dirty_rect());
	CHECK(rect_eq(image.dirty_rect(), Rect{1, 1, 1, 1}));

	image.clear_dirty_rect();
	CHECK(history.redo(image));
	CHECK(history.can_undo());
	CHECK(!history.can_redo());
	CHECK_EQ(image.get_pixel(1, 1), 5);
	CHECK(image.has_dirty_rect());
	CHECK(rect_eq(image.dirty_rect(), Rect{1, 1, 1, 1}));

	CHECK(history.undo(image));
	CHECK(!history.undo(image));
}

TEST_CASE("test_multi_patch_step")
{
	ImageBuffer image(6, 4, 2);
	image.fill(0);

	UndoStep step;
	step.patches.push_back(PixelPatch{Rect{0, 0, 1, 1}, {0}, {5}});
	step.patches.push_back(PixelPatch{Rect{5, 3, 1, 1}, {0}, {7}});

	const ColorIndex five = 5;
	const ColorIndex seven = 7;
	image.write_region(Rect{0, 0, 1, 1}, &five);
	image.write_region(Rect{5, 3, 1, 1}, &seven);

	UndoHistory history;
	history.push(std::move(step));
	CHECK_EQ(history.bytes(), 4u);

	image.clear_dirty_rect();
	CHECK(history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 0);
	CHECK_EQ(image.get_pixel(5, 3), 0);
	CHECK(rect_eq(image.dirty_rect(), Rect{0, 0, 6, 4}));

	CHECK(history.redo(image));
	CHECK_EQ(image.get_pixel(0, 0), 5);
	CHECK_EQ(image.get_pixel(5, 3), 7);
}

TEST_CASE("test_overlapping_patch_order")
{
	ImageBuffer image(1, 1, 2);
	image.fill(0);

	UndoStep step;
	// Same pixel: the second patch is the final state.
	step.patches.push_back(PixelPatch{Rect{0, 0, 1, 1}, {0}, {10}});
	step.patches.push_back(PixelPatch{Rect{0, 0, 1, 1}, {1}, {20}});

	const ColorIndex ten = 10;
	const ColorIndex twenty = 20;
	image.write_region(Rect{0, 0, 1, 1}, &ten);
	image.write_region(Rect{0, 0, 1, 1}, &twenty);

	UndoHistory history;
	history.push(std::move(step));

	CHECK(history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 0);

	CHECK(history.redo(image));
	CHECK_EQ(image.get_pixel(0, 0), 20);
}

TEST_CASE("test_redo_branch_invalidation")
{
	ImageBuffer image(1, 1, 2);
	image.fill(0);

	UndoHistory history;
	history.push(single_pixel_step(0, 0, 0, 1));
	history.push(single_pixel_step(0, 0, 1, 2));
	CHECK_EQ(history.bytes(), 4u);

	CHECK(history.undo(image));
	CHECK(history.can_redo());
	CHECK_EQ(history.bytes(), 4u);

	history.push(single_pixel_step(0, 0, 1, 3));
	CHECK(!history.can_redo());
	CHECK_EQ(history.step_count(), 2u);
	CHECK_EQ(history.bytes(), 4u);
}

TEST_CASE("test_step_limit")
{
	ImageBuffer image(1, 1, 2);
	image.fill(0);

	UndoHistory history(HistoryLimits{1024, 3});
	for (ColorIndex value = 1; value <= 5; ++value)
	{
		image.set_pixel(0, 0, value);
		history.push(single_pixel_step(0, 0, value - 1, value));
	}

	CHECK_EQ(history.step_count(), 3u);
	CHECK_EQ(history.bytes(), 6u);

	CHECK(history.undo(image));
	CHECK(history.undo(image));
	CHECK(history.undo(image));
	CHECK(!history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 2);
}

TEST_CASE("test_byte_limit")
{
	ImageBuffer image(1, 1, 2);
	image.fill(0);

	UndoHistory history(HistoryLimits{4, 100});
	for (ColorIndex value = 1; value <= 3; ++value)
	{
		image.set_pixel(0, 0, value);
		history.push(single_pixel_step(0, 0, value - 1, value));
	}

	CHECK_EQ(history.step_count(), 2u);
	CHECK_EQ(history.bytes(), 4u);

	CHECK(history.undo(image));
	CHECK(history.undo(image));
	CHECK(!history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 1);

	// A step that alone exceeds the budget is kept (soft limit).
	UndoHistory small(HistoryLimits{1, 100});
	small.push(single_pixel_step(0, 0, 1, 2));
	CHECK_EQ(small.step_count(), 1u);
	CHECK_EQ(small.bytes(), 2u);
	CHECK(small.can_undo());

	// Zero steps or zero bytes disable the history.
	UndoHistory no_steps(HistoryLimits{1024, 0});
	no_steps.push(single_pixel_step(0, 0, 0, 1));
	CHECK(!no_steps.can_undo());
	CHECK_EQ(no_steps.bytes(), 0u);

	UndoHistory no_bytes(HistoryLimits{0, 100});
	no_bytes.push(single_pixel_step(0, 0, 0, 1));
	CHECK(!no_bytes.can_undo());
	CHECK_EQ(no_bytes.bytes(), 0u);
}

TEST_CASE("test_set_limits_trims")
{
	ImageBuffer image(1, 1, 2);
	image.fill(0);

	UndoHistory history;
	for (ColorIndex value = 1; value <= 4; ++value)
	{
		image.set_pixel(0, 0, value);
		history.push(single_pixel_step(0, 0, value - 1, value));
	}
	CHECK_EQ(history.step_count(), 4u);

	history.set_limits(HistoryLimits{1024, 2});
	CHECK_EQ(history.step_count(), 2u);
	CHECK_EQ(history.bytes(), 4u);

	CHECK(history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 3);
	CHECK(history.undo(image));
	CHECK_EQ(image.get_pixel(0, 0), 2);
	CHECK(!history.undo(image));

	history.set_limits(HistoryLimits{0, 100});
	CHECK(!history.can_undo());
	CHECK(!history.can_redo());
	CHECK_EQ(history.bytes(), 0u);
}

TEST_CASE("test_invalid_pushes")
{
	UndoHistory history;

	history.push(UndoStep{});
	CHECK(!history.can_undo());

	UndoStep empty_rect;
	empty_rect.patches.push_back(PixelPatch{Rect{0, 0, 0, 1}, {}, {}});
	history.push(std::move(empty_rect));
	CHECK(!history.can_undo());

	UndoStep negative;
	negative.patches.push_back(PixelPatch{Rect{-1, 0, 1, 1}, {0}, {1}});
	history.push(std::move(negative));
	CHECK(!history.can_undo());

	UndoStep bad_size;
	bad_size.patches.push_back(PixelPatch{Rect{0, 0, 2, 2}, {0, 0, 0}, {1, 1, 1}});
	history.push(std::move(bad_size));
	CHECK(!history.can_undo());

	// An invalid push must not destroy the existing redo branch.
	history.push(single_pixel_step(0, 0, 0, 1));
	history.push(single_pixel_step(0, 0, 1, 2));
	ImageBuffer image(1, 1, 2);
	CHECK(history.undo(image));
	CHECK(history.can_redo());
	history.push(UndoStep{});
	CHECK(history.can_redo());
}

TEST_CASE("test_canvas_commit_and_undo")
{
	Canvas canvas(8, 8);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 3), 0);

	DrawBuffer buffer;
	buffer.set_bounds(8, 8);
	buffer.begin(2, 3);
	buffer.ensure_fit(4, 5);
	buffer.clear();

	ColorIndex value = 1;
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			buffer.set_pixel(x, y, value++);
		}
	}

	canvas.commit_draw_buffer(buffer);
	CHECK(!buffer.is_active());
	CHECK(canvas.can_undo());
	CHECK(!canvas.can_redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 3), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 5), 9);

	CHECK(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 3), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 5), 0);
	CHECK(canvas.can_redo());

	CHECK(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 3), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 5), 9);
}

TEST_CASE("test_canvas_noop_and_clipping")
{
	// Active buffer with only transparent pixels: must not create history.
	{
		Canvas canvas(4, 4);
		DrawBuffer buffer;
		buffer.begin(1, 1);
		buffer.clear();
		canvas.commit_draw_buffer(buffer);
		CHECK(!canvas.can_undo());
		CHECK(!buffer.is_active());
	}

	// Inactive buffer: must not create history.
	{
		Canvas canvas(4, 4);
		DrawBuffer buffer;
		canvas.commit_draw_buffer(buffer);
		CHECK(!canvas.can_undo());
	}

	// Buffer partially outside: the patch is clipped to the canvas.
	{
		Canvas canvas(4, 4);
		DrawBuffer buffer;
		buffer.begin(3, 3);
		buffer.ensure_fit(5, 5);
		buffer.set_pixel(0, 0, 7);
		buffer.set_pixel(1, 0, 8);
		buffer.set_pixel(0, 1, 9);
		buffer.set_pixel(1, 1, 10);

		canvas.commit_draw_buffer(buffer);
		CHECK(canvas.can_undo());
		CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 7);
		CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);

		CHECK(canvas.undo());
		CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 0);
		CHECK(!canvas.can_undo());
	}

	// Buffer fully outside: discarded without touching the history.
	{
		Canvas canvas(2, 2);
		DrawBuffer buffer;
		buffer.begin(5, 5);
		buffer.set_pixel(0, 0, 7);
		canvas.commit_draw_buffer(buffer);
		CHECK(!canvas.can_undo());
		CHECK(!buffer.is_active());
	}
}

class FakeTool : public ToolBase
{
public:
	void on_mouse_press(Canvas&, const PaintCore::Vec2&, int) override { drawing = true; }
	void on_mouse_move(Canvas&, const PaintCore::Vec2&) override {}
	void on_mouse_release(Canvas&, const PaintCore::Vec2&, int) override { drawing = false; }
	void on_cancel(Canvas&) override { drawing = false; ++cancels; }
	bool is_drawing() const override { return drawing; }

	bool drawing = false;
	int cancels = 0;
};

TEST_CASE("test_canvas_drawing_guards")
{
	Canvas canvas(4, 4);
	FakeTool tool;
	canvas.set_tool(&tool);

	// Undo during an in-progress stroke cancels it and does not touch the history.
	tool.drawing = true;
	CHECK(canvas.undo());
	CHECK(!tool.drawing);
	CHECK_EQ(tool.cancels, 1);
	CHECK(!canvas.can_undo());

	// Prepare an available redo.
	paint_pixel(canvas, 1, 1, 5);
	CHECK(canvas.undo());
	CHECK(canvas.can_redo());

	// While drawing, redo is rejected.
	tool.drawing = true;
	CHECK(!canvas.redo());
	CHECK(canvas.can_redo());
	tool.drawing = false;

	CHECK(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 5);
}

TEST_CASE("test_canvas_rebranch_and_limits")
{
	Canvas canvas(4, 4);
	CHECK_EQ(canvas.history_limits().max_bytes, 64u * 1024u * 1024u);
	CHECK_EQ(canvas.history_limits().max_steps, 200u);

	paint_pixel(canvas, 0, 0, 1);
	paint_pixel(canvas, 1, 0, 2);
	CHECK(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 0);
	CHECK(canvas.can_redo());

	// A new edit discards the redo branch.
	paint_pixel(canvas, 2, 0, 3);
	CHECK(!canvas.can_redo());

	CHECK(canvas.undo());
	CHECK(canvas.undo());
	CHECK(!canvas.can_undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);

	// set_history_limits trims immediately and Canvas forwards it.
	Canvas limited(8, 8);
	for (int x = 0; x < 4; ++x)
	{
		paint_pixel(limited, x, 0, static_cast<ColorIndex>(x + 1));
	}
	limited.set_history_limits(HistoryLimits{1024 * 1024, 2});
	CHECK_EQ(limited.history_limits().max_steps, 2u);

	CHECK(limited.undo());
	CHECK(limited.undo());
	CHECK(!limited.undo());
	CHECK_EQ(limited.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(limited.get_main_buffer().get_pixel(1, 0), 2);
	CHECK_EQ(limited.get_main_buffer().get_pixel(2, 0), 0);
}
} // namespace
