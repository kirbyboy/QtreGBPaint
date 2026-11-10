// Headless tests for canvas resize and its undo/redo snapshots.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "core/color.h"
#include "paint/paint_session.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/undo_history.h"
#include "tools/base_tool.h"

using PaintCore::ColorIndex;

namespace
{
void set_pixel(PaintSession& canvas, int x, int y, ColorIndex value)
{
	// Seed pixels through the public edit path, then drop the history so the
	// test only observes the resize step.
	DrawBuffer buffer;
	buffer.begin(x, y);
	buffer.set_pixel(0, 0, value);
	canvas.commit_draw_buffer(buffer);
	canvas.clear_history();
}

void commit_pixel(PaintSession& canvas, int x, int y, ColorIndex value)
{
	DrawBuffer buffer;
	buffer.begin(x, y);
	buffer.set_pixel(0, 0, value);
	canvas.commit_draw_buffer(buffer);
}

struct CancelProbeTool : ToolBase
{
	bool canceled = false;

	void on_mouse_press(PaintSession&, const PaintCore::Vec2&, int) override {}
	void on_mouse_move(PaintSession&, const PaintCore::Vec2&) override {}
	void on_mouse_release(PaintSession&, const PaintCore::Vec2&, int) override {}
	void on_cancel(PaintSession&) override { canceled = true; }
};
}

TEST_CASE("PaintSession resize extends with the requested fill index")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 0, 0, 1);
	set_pixel(canvas, 3, 3, 2);

	CHECK(canvas.resize(6, 5, 3));
	CHECK_EQ(canvas.getWidth(), 6);
	CHECK_EQ(canvas.getHeight(), 5);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 0), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 4), 3);
	CHECK(canvas.can_undo());
}

TEST_CASE("PaintSession resize crops from the top-left")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 0, 0, 1);
	set_pixel(canvas, 1, 1, 2);
	set_pixel(canvas, 3, 3, 3);

	CHECK(canvas.resize(2, 2, 0));
	CHECK_EQ(canvas.getWidth(), 2);
	CHECK_EQ(canvas.getHeight(), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 0);
}

TEST_CASE("PaintSession resize ignores no-op and invalid dimensions")
{
	PaintSession canvas(4, 4);

	CHECK_FALSE(canvas.resize(4, 4, 2));
	CHECK_FALSE(canvas.resize(0, 4, 2));
	CHECK_FALSE(canvas.resize(4, 0, 2));
	CHECK_FALSE(canvas.resize(-1, 4, 2));
	CHECK_FALSE(canvas.resize(4, -1, 2));
	CHECK_FALSE(canvas.can_undo());

	CHECK(canvas.resize(1, 1, 2));
	CHECK_EQ(canvas.getWidth(), 1);
	CHECK_EQ(canvas.getHeight(), 1);
	CHECK(canvas.can_undo());
}

TEST_CASE("PaintSession resize cancels the active tool")
{
	PaintSession canvas(4, 4);
	CancelProbeTool tool;
	canvas.set_tool(&tool);

	CHECK(canvas.resize(6, 6, 0));
	CHECK(tool.canceled);
}

TEST_CASE("PaintSession resize undo restores size and pixels")
{
	PaintSession canvas(3, 2);
	set_pixel(canvas, 0, 0, 1);
	set_pixel(canvas, 2, 1, 2);

	CHECK(canvas.resize(5, 4, 3));
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 3), 3);

	CHECK(canvas.undo());
	CHECK_EQ(canvas.getWidth(), 3);
	CHECK_EQ(canvas.getHeight(), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK(canvas.can_redo());

	CHECK(canvas.redo());
	CHECK_EQ(canvas.getWidth(), 5);
	CHECK_EQ(canvas.getHeight(), 4);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 3), 3);
}

TEST_CASE("Resize steps coexist with pixel edits")
{
	PaintSession canvas(3, 3);
	commit_pixel(canvas, 0, 0, 4);
	CHECK(canvas.resize(4, 4, 2));
	commit_pixel(canvas, 3, 3, 5);

	CHECK(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 2);

	CHECK(canvas.undo());
	CHECK_EQ(canvas.getWidth(), 3);
	CHECK_EQ(canvas.getHeight(), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 4);

	CHECK(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);

	CHECK(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 4);

	CHECK(canvas.redo());
	CHECK_EQ(canvas.getWidth(), 4);
	CHECK_EQ(canvas.getHeight(), 4);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 2);

	CHECK(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 5);
}

TEST_CASE("UndoHistory accepts and accounts for canvas resize steps")
{
	ImageBuffer image(2, 2, 2);
	image.fill(0);

	UndoHistory history;
	UndoStep step;
	step.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0, 0, 0}};
	step.canvas_resize = CanvasResize{3, 3, 1};
	history.push(std::move(step));

	REQUIRE(history.can_undo());
	// Only the before snapshot is stored; redo rebuilds the resized image.
	CHECK_EQ(history.bytes(), 4u * sizeof(ColorIndex));

	CHECK(history.undo(image));
	CHECK_EQ(image.width(), 2);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.get_pixel(0, 0), 0);

	CHECK(history.redo(image));
	CHECK_EQ(image.width(), 3);
	CHECK_EQ(image.height(), 3);
	CHECK_EQ(image.get_pixel(0, 0), 0);
	CHECK_EQ(image.get_pixel(2, 0), 1);
	CHECK_EQ(image.get_pixel(0, 2), 1);
	CHECK_EQ(image.get_pixel(2, 2), 1);
}

TEST_CASE("UndoHistory rejects malformed canvas resize steps")
{
	ImageBuffer image(2, 2, 2);

	UndoHistory history;
	UndoStep missing_target;
	missing_target.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0, 0, 0}};
	history.push(std::move(missing_target));
	CHECK_FALSE(history.can_undo());

	UndoStep missing_snapshot;
	missing_snapshot.canvas_resize = CanvasResize{3, 3, 1};
	history.push(std::move(missing_snapshot));
	CHECK_FALSE(history.can_undo());

	UndoStep invalid_target;
	invalid_target.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0, 0, 0}};
	invalid_target.canvas_resize = CanvasResize{0, 3, 1};
	history.push(std::move(invalid_target));
	CHECK_FALSE(history.can_undo());

	UndoStep invalid_snapshot;
	invalid_snapshot.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0}};
	invalid_snapshot.canvas_resize = CanvasResize{3, 3, 1};
	history.push(std::move(invalid_snapshot));
	CHECK_FALSE(history.can_undo());

	UndoStep with_patches;
	with_patches.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0, 0, 0}};
	with_patches.canvas_resize = CanvasResize{3, 3, 1};
	with_patches.patches.push_back(PixelPatch{
		PaintCore::Rect{0, 0, 1, 1}, {0}, {1}});
	history.push(std::move(with_patches));
	CHECK_FALSE(history.can_undo());
	CHECK_EQ(history.bytes(), 0u);
}
