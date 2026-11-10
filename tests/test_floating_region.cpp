// Headless tests for the floating region used by selection and paste.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include "core/color.h"
#include "core/rect.h"
#include "core/vec2.h"
#include "paint/paint_session.h"
#include "paint/canvas_document.h"
#include "paint/floating_region_editor.h"
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "tools/base_tool.h"
#include "tools/selection_tool.h"

using PaintCore::ColorIndex;
using PaintCore::Rect;

namespace
{
bool rect_eq(const Rect& a, const Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void set_pixel(PaintSession& canvas, int x, int y, ColorIndex value)
{
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

ImageBuffer make_content(int width, int height, ColorIndex fill)
{
	ImageBuffer content(width, height, 2);
	content.fill(fill);
	return content;
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

TEST_CASE("begin_selection copies pixels and records the source hole")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 2, 2);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 3, 3}, 3));
	CHECK(canvas.has_floating_region());

	const FloatingRegion& region = canvas.floating_region();
	CHECK(rect_eq(region.rect, Rect{1, 1, 3, 3}));
	CHECK(rect_eq(region.hole, Rect{1, 1, 3, 3}));
	CHECK_EQ(region.hole_fill, 3);
	CHECK_EQ(region.content().width(), 3);
	CHECK_EQ(region.content().height(), 3);
	CHECK_EQ(region.content().get_pixel(0, 0), 1);
	CHECK_EQ(region.content().get_pixel(1, 1), 2);

	// Copying is not a canvas edit until the region is committed.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 2);
	CHECK(canvas.can_undo()); // cancelling a floating region is possible
}

TEST_CASE("begin_selection clips the rectangle to the canvas")
{
	PaintSession canvas(5, 5);

	REQUIRE(canvas.begin_selection(Rect{4, 4, 10, 10}, 0));
	CHECK(rect_eq(canvas.floating_region().rect, Rect{4, 4, 1, 1}));
	canvas.cancel_floating_region();

	REQUIRE(canvas.begin_selection(Rect{-2, -2, 3, 3}, 0));
	CHECK(rect_eq(canvas.floating_region().rect, Rect{0, 0, 1, 1}));
	canvas.cancel_floating_region();

	CHECK_FALSE(canvas.begin_selection(Rect{-10, -10, 2, 2}, 0));
	CHECK_FALSE(canvas.begin_selection(Rect{1, 1, 0, 3}, 0));
	CHECK_FALSE(canvas.has_floating_region());
}

TEST_CASE("moving a floating region keeps at least one pixel visible")
{
	PaintSession canvas(5, 5);
	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0));

	CHECK(canvas.move_floating_region(-100, -100));
	CHECK(rect_eq(canvas.floating_region().rect, Rect{-1, -1, 2, 2}));

	CHECK(canvas.move_floating_region(100, 100));
	CHECK(rect_eq(canvas.floating_region().rect, Rect{4, 4, 2, 2}));

	CHECK_FALSE(canvas.move_floating_region(1, 0));
	CHECK_FALSE(canvas.move_floating_region(0, 1));
}

TEST_CASE("commit fills the hole and applies the moved content atomically")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 1, 2);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 3));
	REQUIRE(canvas.move_floating_region(1, 0));
	REQUIRE(canvas.commit_floating_region());

	CHECK_FALSE(canvas.has_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 1), 2);
	CHECK(canvas.can_undo());

	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 1), 0);

	REQUIRE(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 1), 2);
}

TEST_CASE("cancel discards the floating region without touching pixels")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 1, 2);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 3));
	REQUIRE(canvas.move_floating_region(1, 0));
	canvas.cancel_floating_region();

	CHECK_FALSE(canvas.has_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 1), 0);
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("committing without changes does not create history")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 3));
	CHECK_FALSE(canvas.commit_floating_region());
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
}

TEST_CASE("content moved outside the canvas is clipped on commit")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(3, 3, 1);
	content.set_pixel(0, 0, 2);
	content.set_pixel(1, 1, 3);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{3, 3}));
	REQUIRE(canvas.commit_floating_region());

	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);
	CHECK(canvas.can_undo());
}

TEST_CASE("a paste fully outside the canvas is discarded")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(2, 2, 1);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{-5, -5}));
	CHECK_FALSE(canvas.commit_floating_region());
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);
}

TEST_CASE("paste skips transparent pixels and leaves no hole")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(2, 2, PaintCore::TRANSPARENT_INDEX);
	content.set_pixel(0, 0, 1);
	content.set_pixel(1, 1, 2);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{1, 1}));
	CHECK(rect_eq(canvas.floating_region().hole, Rect{}));
	REQUIRE(canvas.commit_floating_region());

	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
}

TEST_CASE("starting a new floating region commits the previous one")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 0, 0, 1);

	REQUIRE(canvas.begin_selection(Rect{0, 0, 1, 1}, 2));
	REQUIRE(canvas.move_floating_region(1, 0));

	ImageBuffer content = make_content(1, 1, 4);
	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{3, 3}));

	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 1);
	CHECK(canvas.can_undo());

	canvas.cancel_floating_region();
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 0);
}

TEST_CASE("switching tools commits the floating region")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 0, 0, 1);

	CancelProbeTool first_tool;
	CancelProbeTool second_tool;
	canvas.set_tool(&first_tool);

	REQUIRE(canvas.begin_selection(Rect{0, 0, 1, 1}, 2));
	REQUIRE(canvas.move_floating_region(1, 0));

	canvas.set_tool(&second_tool);
	CHECK(first_tool.canceled);
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 1);
	CHECK(canvas.can_undo());
}

TEST_CASE("resize commits the floating region before changing dimensions")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 4));
	REQUIRE(canvas.move_floating_region(1, 0));
	REQUIRE(canvas.resize(6, 6, 0));

	CHECK_EQ(canvas.getWidth(), 6);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 4);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);

	// First undo reverts the resize, second one reverts the selection commit.
	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.getWidth(), 5);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 4);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);

	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
}

TEST_CASE("resize with the same dimensions still commits the floating region")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 0, 0, 1);

	REQUIRE(canvas.begin_selection(Rect{0, 0, 1, 1}, 2));
	REQUIRE(canvas.move_floating_region(1, 0));

	// The PaintSession Size dialog calls resize even when the user keeps the size;
	// that is still a confirming action.
	CHECK_FALSE(canvas.resize(4, 4, 0));
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 1);
	CHECK(canvas.can_undo());
}

TEST_CASE("undo cancels a floating region and disables redo")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 2));
	CHECK(canvas.can_undo());
	CHECK_FALSE(canvas.can_redo());
	CHECK(canvas.undo());
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
}

TEST_CASE("redo is blocked while a floating region exists")
{
	PaintSession canvas(4, 4);
	commit_pixel(canvas, 0, 0, 4);
	REQUIRE(canvas.undo());
	REQUIRE(canvas.can_redo());

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 2));
	CHECK_FALSE(canvas.can_redo());
	CHECK_FALSE(canvas.redo());

	canvas.cancel_floating_region();
	CHECK(canvas.can_redo());
	REQUIRE(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 4);
}

TEST_CASE("SelectionTool exposes normal and transparent background modes")
{
	SelectionTool tool;
	const std::vector<ToolOption>& options = tool.options();

	REQUIRE(options.size() == 1u);
	CHECK(options[0].id == "mode");
	CHECK(options[0].type == ToolOptionType::Choice);
	CHECK(options[0].visible_in_options_bar);
	CHECK_EQ(options[0].value,
	         static_cast<int>(SelectionTool::Mode::Normal));
	REQUIRE(options[0].choices.size() == 2u);
	CHECK_EQ(options[0].choices[0].value,
	         static_cast<int>(SelectionTool::Mode::Normal));
	CHECK(options[0].choices[0].icon == "selection.normal");
	CHECK_EQ(options[0].choices[1].value,
	         static_cast<int>(SelectionTool::Mode::Transparent));
	CHECK(options[0].choices[1].icon == "selection.transparent");

	CHECK(tool.set_option("mode",
	                      static_cast<int>(SelectionTool::Mode::Transparent)));
	CHECK_EQ(tool.option_value("mode"),
	         static_cast<int>(SelectionTool::Mode::Transparent));
	CHECK_FALSE(tool.set_option("mode", 99));
}

TEST_CASE("transparent background mode stores raw pixels and a live mask")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, true));
	CHECK(canvas.floating_region().transparent_background);
	CHECK_EQ(canvas.floating_region().transparent_color, 0);
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 0);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 0), 1);
	CHECK_EQ(canvas.floating_region().hole_fill, 0);

	canvas.cancel_floating_region();
	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, false));
	CHECK_FALSE(canvas.floating_region().transparent_background);
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 0);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 0), 1);
}

TEST_CASE("transparent background selection preserves the destination")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 0); // becomes transparent
	set_pixel(canvas, 2, 1, 1);
	set_pixel(canvas, 2, 2, 3); // destination under the transparent pixel

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, true));
	REQUIRE(canvas.move_floating_region(1, 1));
	REQUIRE(canvas.commit_floating_region());

	// The hole uses the background color.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
	// The transparent content pixel does not overwrite the destination.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	// The opaque content pixel is written.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
	CHECK(canvas.can_undo());
}

TEST_CASE("transparent mode without background pixels commits as a no-op")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 0, true));
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 1);
	CHECK_FALSE(canvas.commit_floating_region());
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("an all-transparent selection commits as a no-op")
{
	PaintSession canvas(4, 4);
	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0, true));

	CHECK_FALSE(canvas.commit_floating_region());
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
}

TEST_CASE("SelectionTool passes transparent mode to PaintSession")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);

	SelectionTool tool;
	tool.set_colors(3, 0);
	REQUIRE(tool.set_option("mode",
	                        static_cast<int>(SelectionTool::Mode::Transparent)));
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, PaintCore::Vec2{1, 1}, 1);
	tool.on_mouse_release(canvas, PaintCore::Vec2{2, 1}, 1);

	REQUIRE(canvas.has_floating_region());
	CHECK(canvas.floating_region().transparent_background);
	CHECK_EQ(canvas.floating_region().transparent_color, 0);
	CHECK(rect_eq(canvas.floating_region().rect, Rect{1, 1, 2, 1}));
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 0);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 0), 1);
}

TEST_CASE("transparency can be toggled on an existing selection")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);
	set_pixel(canvas, 2, 2, 3); // destination under the transparent pixel

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, false));
	CHECK_FALSE(canvas.floating_region().transparent_background);

	REQUIRE(canvas.set_floating_transparency(true, 0));
	CHECK(canvas.floating_region().transparent_background);
	CHECK_EQ(canvas.floating_region().transparent_color, 0);
	// The content stays raw: only the live mask changed.
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 0);

	CHECK_FALSE(canvas.set_floating_transparency(true, 0));
	REQUIRE(canvas.move_floating_region(1, 1));
	REQUIRE(canvas.commit_floating_region());

	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
}

TEST_CASE("enabling a matching transparency mask commits as a pixel change")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 0, false));
	// can_undo() would be true because the floating region is cancellable;
	// check the document history itself.
	CHECK_FALSE(canvas.document().can_undo());

	// The mask makes the content pixel transparent, so the hole fill wins.
	REQUIRE(canvas.set_floating_transparency(true, 1));
	REQUIRE(canvas.commit_floating_region());

	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK(canvas.can_undo());
	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
}

TEST_CASE("floating content and mask changes bump the region revision")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 1, 1}, 0));
	const std::uint64_t initial = canvas.floating_region().revision();

	REQUIRE(canvas.invert_floating_region_colors());
	CHECK(canvas.floating_region().revision() > initial);
	const std::uint64_t after_invert =
		canvas.floating_region().revision();

	REQUIRE(canvas.set_floating_transparency(true, 0));
	CHECK(canvas.floating_region().revision() > after_invert);
	const std::uint64_t after_mask =
		canvas.floating_region().revision();

	// Repeating the same mask is a no-op and must not invalidate the texture.
	CHECK_FALSE(canvas.set_floating_transparency(true, 0));
	CHECK_EQ(canvas.floating_region().revision(), after_mask);
}

TEST_CASE("disabling transparency makes background pixels opaque again")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, false));
	REQUIRE(canvas.set_floating_transparency(true, 0));
	REQUIRE(canvas.set_floating_transparency(false, 0));
	CHECK_FALSE(canvas.floating_region().transparent_background);
	REQUIRE(canvas.move_floating_region(1, 1));
	REQUIRE(canvas.commit_floating_region());

	// The background-colored content pixel is written again.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
}

TEST_CASE("copy keeps raw pixels and paste uses the live transparency mask")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0, true));
	REQUIRE(canvas.copy_to_clipboard());
	CHECK_EQ(canvas.clipboard().get_pixel(0, 0), 0);
	CHECK_EQ(canvas.clipboard().get_pixel(1, 0), 1);
	canvas.cancel_floating_region();

	REQUIRE(canvas.paste_from_clipboard(PaintCore::Vec2{2, 2}));
	CHECK_FALSE(canvas.floating_region().transparent_background);
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 0);

	REQUIRE(canvas.set_floating_transparency(true, 0));
	REQUIRE(canvas.commit_floating_region());

	// The pasted background pixel is transparent and preserves the destination.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
}

TEST_CASE("set_floating_transparency is a no-op without a floating region")
{
	PaintSession canvas(4, 4);
	CHECK_FALSE(canvas.set_floating_transparency(true, 0));
}

TEST_CASE("transparency can be toggled on a pasted region")
{
	PaintSession canvas(6, 6);
	ImageBuffer content = make_content(2, 1, 0);
	content.set_pixel(1, 0, 1);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{2, 2}));
	CHECK_FALSE(canvas.floating_region().transparent_background);
	REQUIRE(canvas.set_floating_transparency(true, 0));
	REQUIRE(canvas.commit_floating_region());

	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
}

TEST_CASE("SelectionTool creates only the marquee and hands it to PaintSession")
{
	PaintSession canvas(8, 8);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 1, 2);

	SelectionTool tool;
	tool.set_colors(3, 2);
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, PaintCore::Vec2{1, 1}, 1);
	CHECK(tool.is_drawing());
	REQUIRE(tool.has_marquee());
	CHECK(rect_eq(tool.marquee_rect(), Rect{1, 1, 1, 1}));

	tool.on_mouse_move(canvas, PaintCore::Vec2{3, 2});
	CHECK(rect_eq(tool.marquee_rect(), Rect{1, 1, 3, 2}));

	tool.on_mouse_release(canvas, PaintCore::Vec2{3, 2}, 1);
	CHECK_FALSE(tool.is_drawing());
	CHECK_FALSE(tool.has_marquee());
	REQUIRE(canvas.has_floating_region());
	CHECK(rect_eq(canvas.floating_region().rect, Rect{1, 1, 3, 2}));
	CHECK(rect_eq(canvas.floating_region().hole, Rect{1, 1, 3, 2}));
	CHECK_EQ(canvas.floating_region().hole_fill, 2);
}

TEST_CASE("SelectionTool normalizes reverse drags and cancellations")
{
	PaintSession canvas(8, 8);

	SelectionTool tool;
	canvas.set_tool(&tool);

	tool.on_mouse_press(canvas, PaintCore::Vec2{4, 4}, 1);
	tool.on_mouse_move(canvas, PaintCore::Vec2{2, 2});
	CHECK(rect_eq(tool.marquee_rect(), Rect{2, 2, 3, 3}));
	tool.on_cancel(canvas);

	CHECK_FALSE(tool.is_drawing());
	CHECK_FALSE(tool.has_marquee());
	CHECK_FALSE(canvas.has_floating_region());

	tool.on_mouse_press(canvas, PaintCore::Vec2{4, 4}, 1);
	tool.on_mouse_move(canvas, PaintCore::Vec2{2, 2});
	tool.on_mouse_release(canvas, PaintCore::Vec2{2, 2}, 1);
	REQUIRE(canvas.has_floating_region());
	CHECK(rect_eq(canvas.floating_region().rect, Rect{2, 2, 3, 3}));
}

TEST_CASE("a selection that only moves creates history on commit")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 1);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0));
	CHECK_FALSE(canvas.move_floating_region(0, 0));
	CHECK_FALSE(canvas.commit_floating_region());
	CHECK_FALSE(canvas.can_undo());

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0));
	REQUIRE(canvas.move_floating_region(1, 0));
	REQUIRE(canvas.commit_floating_region());
	CHECK(canvas.can_undo());
}

TEST_CASE("invert colors works without moving the selection and creates history")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 0);
	set_pixel(canvas, 2, 1, 1);
	set_pixel(canvas, 1, 2, 2);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0));

	REQUIRE(canvas.invert_floating_region_colors());
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 3);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 0), 2);
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 1), 1);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 0);

	REQUIRE(canvas.commit_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 3);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	CHECK(canvas.can_undo());

	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 2), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
}

TEST_CASE("invert colors skips transparent paste pixels")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(2, 2, PaintCore::TRANSPARENT_INDEX);
	content.set_pixel(0, 0, 1);
	content.set_pixel(1, 1, 2);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{1, 1}));
	REQUIRE(canvas.invert_floating_region_colors());
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 2);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 0),
	         PaintCore::TRANSPARENT_INDEX);

	REQUIRE(canvas.commit_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
}

TEST_CASE("copy keeps the floating region and stores its pixels")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 1, 2);

	CHECK_FALSE(canvas.copy_to_clipboard());
	CHECK_FALSE(canvas.has_clipboard());

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0));
	REQUIRE(canvas.copy_to_clipboard());

	CHECK(canvas.has_floating_region());
	CHECK(canvas.has_clipboard());
	CHECK_EQ(canvas.clipboard().width(), 2);
	CHECK_EQ(canvas.clipboard().height(), 1);
	CHECK_EQ(canvas.clipboard().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.clipboard().get_pixel(1, 0), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
}

TEST_CASE("delete fills the original hole and discards moved content")
{
	PaintSession canvas(6, 6);
	set_pixel(canvas, 1, 1, 2);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 1));
	REQUIRE(canvas.move_floating_region(2, 2));
	CHECK(rect_eq(canvas.floating_region().rect, Rect{3, 3, 2, 2}));

	REQUIRE(canvas.delete_floating_region());

	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.has_clipboard());
	// The original hole is filled with the saved background.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 2), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 1);
	// The destination is untouched because the content was discarded.
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 3), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 0);
	CHECK(canvas.can_undo());

	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
	REQUIRE(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
}

TEST_CASE("deleting a pasted floating region only discards it")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(2, 2, 1);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{1, 1}));
	REQUIRE(canvas.delete_floating_region());

	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.has_clipboard());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("delete without a floating region is rejected")
{
	PaintSession canvas(3, 3);
	CHECK_FALSE(canvas.delete_floating_region());
}

TEST_CASE("cut fills the selection hole with background and can be undone")
{
	PaintSession canvas(4, 4);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 1, 2);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 1}, 0));
	REQUIRE(canvas.cut_to_clipboard());

	CHECK_FALSE(canvas.has_floating_region());
	CHECK(canvas.has_clipboard());
	CHECK_EQ(canvas.clipboard().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.clipboard().get_pixel(1, 0), 2);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
	CHECK(canvas.can_undo());

	REQUIRE(canvas.undo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 2);
	REQUIRE(canvas.redo());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 0);
}

TEST_CASE("cutting a pasted floating region only discards it")
{
	PaintSession canvas(4, 4);
	ImageBuffer content = make_content(2, 2, 1);

	REQUIRE(canvas.begin_paste(content, PaintCore::Vec2{1, 1}));
	REQUIRE(canvas.cut_to_clipboard());

	CHECK_FALSE(canvas.has_floating_region());
	CHECK(canvas.has_clipboard());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("paste from clipboard creates a floating region at the requested position")
{
	PaintSession canvas(5, 5);
	set_pixel(canvas, 1, 1, 1);
	set_pixel(canvas, 2, 2, 3);

	REQUIRE(canvas.begin_selection(Rect{1, 1, 2, 2}, 0));
	REQUIRE(canvas.copy_to_clipboard());
	canvas.cancel_floating_region();

	REQUIRE(canvas.paste_from_clipboard(PaintCore::Vec2{2, 1}));
	CHECK(canvas.has_floating_region());
	CHECK(rect_eq(canvas.floating_region().rect, Rect{2, 1, 2, 2}));
	CHECK(rect_eq(canvas.floating_region().hole, Rect{}));
	CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 1);
	CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 3);

	REQUIRE(canvas.commit_floating_region());
	CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 3);
	CHECK(canvas.can_undo());
}

TEST_CASE("paste without clipboard is rejected")
{
	PaintSession canvas(4, 4);
	CHECK_FALSE(canvas.paste_from_clipboard(PaintCore::Vec2{0, 0}));
	CHECK_FALSE(canvas.has_floating_region());
	CHECK_FALSE(canvas.can_undo());
}

TEST_CASE("FloatingRegionEditor works directly on a CanvasDocument")
{
	CanvasDocument document(5, 5);
	DrawBuffer seed;
	seed.begin(1, 1);
	seed.set_pixel(0, 0, 1);
	document.commit_draw_buffer(seed);
	document.clear_history();

	FloatingRegionEditor editor(document);
	REQUIRE(editor.begin_selection(Rect{1, 1, 1, 1}, 0));
	REQUIRE(editor.move_floating_region(1, 0));
	REQUIRE(editor.commit_floating_region());

	CHECK_EQ(document.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_EQ(document.get_main_buffer().get_pixel(2, 1), 1);
	CHECK(document.can_undo());

	REQUIRE(document.undo());
	CHECK_EQ(document.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(document.get_main_buffer().get_pixel(2, 1), 0);
}

TEST_CASE("SelectionTool constrains the marquee to a square with Shift")
{
	PaintSession canvas(8, 8);
	SelectionTool tool;
	canvas.set_tool(&tool);
	PaintCore::Modifiers modifiers;
	modifiers.shift = true;
	canvas.set_modifiers(modifiers);

	tool.on_mouse_press(canvas, PaintCore::Vec2{1, 1}, 1);
	tool.on_mouse_move(canvas, PaintCore::Vec2{5, 3});
	tool.on_mouse_release(canvas, PaintCore::Vec2{5, 3}, 1);

	REQUIRE(canvas.has_floating_region());
	CHECK(rect_eq(canvas.floating_region().rect, Rect{1, 1, 3, 3}));
}

