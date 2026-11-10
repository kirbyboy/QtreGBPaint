// Headless tests for the Pick Color tool and its PaintBackend integration.
// Run with `make -C tests test-pick_color` or `make test` from the root.

#include "doctest/doctest.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/color.h"
#include "core/vec2.h"
#include "paint/image_buffer.h"
#include "paint/paint_backend.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"
#include "tools/fill_tool.h"
#include "tools/pick_color_tool.h"
#include "tools/selection_tool.h"

using PaintCore::ColorIndex;
using PaintCore::Vec2;

namespace
{
void add_test_buttons(PaintBackend& backend)
{
	const Palette gray = Palette::grayscale(3);
	backend.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                  PaintCore::CursorType::Fill,
	                  "", gray);
	backend.addButton("Selection", std::make_shared<SelectionTool>(),
	                  PaintCore::CursorType::Crosshair,
	                  "", gray);
	backend.addButton("Pick Color", std::make_shared<PickColorTool>(),
	                  PaintCore::CursorType::Pick,
	                  "", gray);
}

void load_pixels(PaintSession& session, int width, int height,
                 const std::vector<ColorIndex>& pixels)
{
	ImageBuffer image(width, height, 2);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			image.set_pixel(
				x, y,
				pixels[static_cast<std::size_t>(y) *
				           static_cast<std::size_t>(width) +
				       static_cast<std::size_t>(x)]);
		}
	}
	session.load_image(std::move(image));
}
}

TEST_CASE("PickColorTool returns left and right picks once")
{
	const int width = 3;
	const int height = 2;
	PaintSession session(width, height);
	load_pixels(session, width, height,
	            {
	                0, 2, 0,
	                0, 0, 3,
	            });

	PickColorTool tool;
	CHECK(tool.accepts_button(ToolBase::kLeftButton));
	CHECK(tool.accepts_button(ToolBase::kRightButton));
	session.set_tool(&tool);

	session.handle_mouse_press({1, 0}, ToolBase::kLeftButton);
	ToolBase::ColorPick pick;
	REQUIRE(tool.take_color_pick(pick));
	CHECK_EQ(pick.color, 2);
	CHECK_FALSE(pick.background);
	CHECK_FALSE(tool.take_color_pick(pick));

	session.handle_mouse_press({2, 1}, ToolBase::kRightButton);
	REQUIRE(tool.take_color_pick(pick));
	CHECK_EQ(pick.color, 3);
	CHECK(pick.background);
}

TEST_CASE("PickColorTool ignores outside, transparent and cancelled picks")
{
	PaintSession session(3, 2);
	load_pixels(session, 3, 2,
	            {
	                0, 2, 0,
	                0, 0, 0,
	            });

	PickColorTool tool;
	session.set_tool(&tool);
	ToolBase::ColorPick pick;

	session.handle_mouse_press({-1, 0}, ToolBase::kLeftButton);
	CHECK_FALSE(tool.take_color_pick(pick));
	session.handle_mouse_press({3, 0}, ToolBase::kLeftButton);
	CHECK_FALSE(tool.take_color_pick(pick));
	session.handle_mouse_press({0, 2}, ToolBase::kLeftButton);
	CHECK_FALSE(tool.take_color_pick(pick));

	PaintSession transparent_session(2, 2);
	ImageBuffer transparent(2, 2, 8);
	transparent.fill(0);
	transparent.set_pixel(1, 1, PaintCore::TRANSPARENT_INDEX);
	transparent_session.load_image(std::move(transparent));
	PickColorTool transparent_tool;
	transparent_session.set_tool(&transparent_tool);
	transparent_session.handle_mouse_press({1, 1}, ToolBase::kLeftButton);
	CHECK_FALSE(transparent_tool.take_color_pick(pick));

	session.handle_mouse_press({1, 0}, ToolBase::kLeftButton);
	session.set_tool(nullptr);
	CHECK_FALSE(tool.take_color_pick(pick));
}

TEST_CASE("PaintBackend applies picks and restores the last editing tool")
{
	PaintBackend backend(nullptr);
	add_test_buttons(backend);
	load_pixels(backend.session(), 4, 2,
	            {
	                0, 2, 0, 0,
	                0, 0, 3, 0,
	            });

	REQUIRE(backend.selectButton("Fill") != nullptr);
	REQUIRE(backend.selectButton("Selection") != nullptr);
	REQUIRE(backend.selectButton("Pick Color") != nullptr);
	CHECK(backend.activeCursor() == PaintCore::CursorType::Pick);

	backend.session().handle_mouse_press({1, 0}, ToolBase::kLeftButton);
	CHECK(backend.apply_pending_color_pick());
	CHECK_EQ(backend.foreground_index(), 2);
	CHECK(backend.background_index() == 0);
	CHECK(backend.activeButton() == "Fill");
	CHECK_FALSE(backend.can_undo());

	REQUIRE(backend.selectButton("Pick Color") != nullptr);
	backend.session().handle_mouse_press({2, 1}, ToolBase::kRightButton);
	CHECK(backend.apply_pending_color_pick());
	CHECK_EQ(backend.background_index(), 3);
	CHECK_EQ(backend.foreground_index(), 2);
	CHECK(backend.activeButton() == "Fill");
	CHECK_FALSE(backend.can_undo());
}

TEST_CASE("Pick Color stays active when there is no previous editing tool")
{
	PaintBackend backend(nullptr);
	add_test_buttons(backend);
	load_pixels(backend.session(), 2, 2,
	            {
	                0, 0,
	                0, 2,
	            });

	REQUIRE(backend.selectButton("Pick Color") != nullptr);
	backend.session().handle_mouse_press({1, 1}, ToolBase::kLeftButton);
	CHECK(backend.apply_pending_color_pick());
	CHECK_EQ(backend.foreground_index(), 2);
	CHECK(backend.activeButton() == "Pick Color");
}

TEST_CASE("Selecting Pick Color confirms a floating selection")
{
	PaintBackend backend(nullptr);
	add_test_buttons(backend);
	load_pixels(backend.session(), 4, 4,
	            std::vector<ColorIndex>(16, 1));

	REQUIRE(backend.selectButton("Fill") != nullptr);
	REQUIRE(backend.session().begin_selection({0, 0, 3, 3}, 0));
	CHECK(backend.session().has_floating_region());

	REQUIRE(backend.selectButton("Pick Color") != nullptr);
	CHECK_FALSE(backend.session().has_floating_region());
	CHECK(backend.activeButton() == "Pick Color");
}
