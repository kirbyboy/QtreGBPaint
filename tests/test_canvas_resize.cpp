// Headless tests for canvas resize and its undo/redo snapshots.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "core/color.h"
#include "paint/canvas.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "paint/undo_history.h"
#include "tools/base_tool.h"

using PaintCore::ColorIndex;

namespace
{
void set_pixel(Canvas& canvas, int x, int y, ColorIndex value)
{
	canvas.get_main_buffer().set_pixel(x, y, value);
}

void commit_pixel(Canvas& canvas, int x, int y, ColorIndex value)
{
	DrawBuffer buffer;
	buffer.begin(x, y);
	buffer.set_pixel(0, 0, value);
	canvas.commit_draw_buffer(buffer);
}

struct CancelProbeTool : ToolBase
{
	bool canceled = false;

	void on_mouse_press(Canvas&, const PaintCore::Vec2&, int) override {}
	void on_mouse_move(Canvas&, const PaintCore::Vec2&) override {}
	void on_mouse_release(Canvas&, const PaintCore::Vec2&, int) override {}
	void on_cancel(Canvas&) override { canceled = true; }
};
}

TEST_CASE("Canvas resize extends with the requested fill index")
{
	Canvas canvas(4, 4);
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

TEST_CASE("Canvas resize crops from the top-left")
{
	Canvas canvas(4, 4);
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

TEST_CASE("Canvas resize ignores no-op and invalid dimensions")
{
	Canvas canvas(4, 4);

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

TEST_CASE("Canvas resize cancels the active tool")
{
	Canvas canvas(4, 4);
	CancelProbeTool tool;
	canvas.set_tool(&tool);

	CHECK(canvas.resize(6, 6, 0));
	CHECK(tool.canceled);
}

TEST_CASE("Canvas resize undo restores size and pixels")
{
	Canvas canvas(3, 2);
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
	Canvas canvas(3, 3);
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

TEST_CASE("UndoHistory accepts and accounts for canvas snapshots")
{
	ImageBuffer image(2, 2, 2);
	image.fill(0);

	UndoHistory history;
	UndoStep step;
	step.before_canvas = CanvasSnapshot{2, 2, 2, {0, 0, 0, 0}};
	step.after_canvas = CanvasSnapshot{3, 3, 2, {1, 1, 1, 1, 1, 1, 1, 1, 1}};
	history.push(std::move(step));

	REQUIRE(history.can_undo());
	CHECK_EQ(history.bytes(), 13u * sizeof(ColorIndex));

	CHECK(history.undo(image));
	CHECK_EQ(image.width(), 2);
	CHECK_EQ(image.height(), 2);

	CHECK(history.redo(image));
	CHECK_EQ(image.width(), 3);
	CHECK_EQ(image.height(), 3);
	CHECK_EQ(image.get_pixel(2, 2), 1);
}
