// Headless tests for whole-image flips/rotations and their history steps.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <utility>
#include <vector>

#include "core/color.h"
#include "core/image_transform.h"
#include "core/rect.h"
#include "paint/canvas_document.h"
#include "paint/image_buffer.h"
#include "paint/image_transform.h"
#include "paint/paint_session.h"
#include "paint/undo_history.h"
#include "tools/draw_tool.h"

using PaintCore::ColorIndex;
using PaintCore::ImageTransform;
using PaintCore::Rect;

namespace
{
ImageBuffer make_pattern(int width, int height,
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
	return image;
}

bool rect_eq(const Rect& a, const Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Minimal DrawTool that leaves a single pixel at the current pointer.
struct StrokeProbeTool : DrawTool
{
	StrokeProbeTool() : DrawTool(1, 0) {}

	void draw_preview(DrawBuffer& buffer,
	                  const PaintCore::Modifiers&) override
	{
		buffer.ensure_fit(m_start_pos.x, m_start_pos.y);
		buffer.ensure_fit(m_current_pos.x, m_current_pos.y);
		buffer.set_pixel(m_current_pos.x - buffer.x(),
		                 m_current_pos.y - buffer.y(), 1);
	}
};

void check_transform_pixels()
{
	const std::vector<ColorIndex> source_pixels{
		0, 1, 2,
		3, 4, 5,
	};
	const ImageBuffer source = make_pattern(3, 2, source_pixels);

	const auto transformed = [&](ImageTransform transform)
	{
		return PaintCore::transform_image(source, transform);
	};

	const ImageBuffer horizontal =
		transformed(ImageTransform::FlipHorizontal);
	CHECK_EQ(horizontal.width(), 3);
	CHECK_EQ(horizontal.height(), 2);
	CHECK(horizontal.data() ==
	      std::vector<ColorIndex>({2, 1, 0, 5, 4, 3}));

	const ImageBuffer vertical =
		transformed(ImageTransform::FlipVertical);
	CHECK_EQ(vertical.width(), 3);
	CHECK_EQ(vertical.height(), 2);
	CHECK(vertical.data() ==
	      std::vector<ColorIndex>({3, 4, 5, 0, 1, 2}));

	const ImageBuffer rotate90 = transformed(ImageTransform::Rotate90);
	CHECK_EQ(rotate90.width(), 2);
	CHECK_EQ(rotate90.height(), 3);
	CHECK(rotate90.data() ==
	      std::vector<ColorIndex>({3, 0, 4, 1, 5, 2}));

	const ImageBuffer rotate180 = transformed(ImageTransform::Rotate180);
	CHECK_EQ(rotate180.width(), 3);
	CHECK_EQ(rotate180.height(), 2);
	CHECK(rotate180.data() ==
	      std::vector<ColorIndex>({5, 4, 3, 2, 1, 0}));

	const ImageBuffer rotate270 = transformed(ImageTransform::Rotate270);
	CHECK_EQ(rotate270.width(), 2);
	CHECK_EQ(rotate270.height(), 3);
	CHECK(rotate270.data() ==
	      std::vector<ColorIndex>({2, 5, 1, 4, 0, 3}));
}

void check_inverse_round_trip()
{
	const std::vector<ColorIndex> source_pixels{
		0, 1, 2,
		3, 4, 5,
	};
	const ImageBuffer source = make_pattern(3, 2, source_pixels);
	const ImageTransform transforms[] = {
		ImageTransform::FlipHorizontal,
		ImageTransform::FlipVertical,
		ImageTransform::Rotate90,
		ImageTransform::Rotate180,
		ImageTransform::Rotate270,
	};

	for (ImageTransform transform : transforms)
	{
		const ImageBuffer transformed =
			PaintCore::transform_image(source, transform);
		const ImageBuffer restored = PaintCore::transform_image(
			transformed, PaintCore::inverse_image_transform(transform));
		CHECK_EQ(restored.width(), source.width());
		CHECK_EQ(restored.height(), source.height());
		CHECK_EQ(restored.bitsPerPixel(), source.bitsPerPixel());
		CHECK(restored.data() == source.data());
	}
}
} // namespace

TEST_CASE("image_transform applies the expected pixel permutations")
{
	check_transform_pixels();
	check_inverse_round_trip();
}

TEST_CASE("image_transform metadata")
{
	CHECK(PaintCore::is_valid_image_transform(
		ImageTransform::FlipHorizontal));
	CHECK(PaintCore::is_valid_image_transform(
		ImageTransform::Rotate270));
	CHECK_FALSE(PaintCore::is_valid_image_transform(
		static_cast<ImageTransform>(99)));

	CHECK_FALSE(PaintCore::image_transform_swaps_axes(
		ImageTransform::FlipHorizontal));
	CHECK_FALSE(PaintCore::image_transform_swaps_axes(
		ImageTransform::Rotate180));
	CHECK(PaintCore::image_transform_swaps_axes(
		ImageTransform::Rotate90));
	CHECK(PaintCore::image_transform_swaps_axes(
		ImageTransform::Rotate270));

	CHECK(PaintCore::inverse_image_transform(ImageTransform::FlipVertical) ==
	      ImageTransform::FlipVertical);
	CHECK(PaintCore::inverse_image_transform(ImageTransform::Rotate90) ==
	      ImageTransform::Rotate270);
	CHECK(PaintCore::inverse_image_transform(ImageTransform::Rotate270) ==
	      ImageTransform::Rotate90);
	CHECK(PaintCore::inverse_image_transform(ImageTransform::Rotate180) ==
	      ImageTransform::Rotate180);
}

TEST_CASE("CanvasDocument transform steps undo by inverse permutation")
{
	const std::vector<ColorIndex> original{0, 1, 2, 3, 4, 5};
	const ImageTransform transforms[] = {
		ImageTransform::FlipHorizontal,
		ImageTransform::FlipVertical,
		ImageTransform::Rotate90,
		ImageTransform::Rotate180,
		ImageTransform::Rotate270,
	};

	for (ImageTransform transform : transforms)
	{
		CanvasDocument document(3, 2);
		REQUIRE(document.apply_pixel_patch(Rect{0, 0, 3, 2}, original));
		document.clear_history();

		REQUIRE(document.transform_image(transform));
		const ImageBuffer& after = document.get_main_buffer();
		const int after_width = after.width();
		const int after_height = after.height();
		const std::vector<ColorIndex> after_pixels = after.data();
		CHECK(document.can_undo());
		CHECK(rect_eq(document.take_dirty_rect(),
		               Rect{0, 0, after_width, after_height}));

		REQUIRE(document.undo());
		CHECK_EQ(document.getWidth(), 3);
		CHECK_EQ(document.getHeight(), 2);
		CHECK(document.get_main_buffer().data() == original);
		CHECK_FALSE(document.can_undo());
		CHECK(document.can_redo());
		CHECK(rect_eq(document.take_dirty_rect(), Rect{0, 0, 3, 2}));

		REQUIRE(document.redo());
		CHECK_EQ(document.getWidth(), after_width);
		CHECK_EQ(document.getHeight(), after_height);
		CHECK(document.get_main_buffer().data() == after_pixels);
		CHECK_FALSE(document.can_redo());
	}
}

TEST_CASE("symmetric flips do not create history")
{
	CanvasDocument document(2, 2);
	REQUIRE(document.apply_pixel_patch(
		Rect{0, 0, 2, 2}, {1, 1, 2, 2}));
	document.clear_history();

	CHECK_FALSE(document.transform_image(ImageTransform::FlipHorizontal));
	CHECK_FALSE(document.can_undo());
	CHECK(document.get_main_buffer().data() ==
	      std::vector<ColorIndex>({1, 1, 2, 2}));
	CHECK_EQ(document.getWidth(), 2);
	CHECK_EQ(document.getHeight(), 2);
}

TEST_CASE("transform undo steps store no pixel copy")
{
	UndoStep step;
	step.canvas_transform = ImageTransform::Rotate90;
	CHECK_EQ(step.bytes(), 0u);

	UndoHistory history;
	UndoStep invalid;
	invalid.canvas_transform = static_cast<ImageTransform>(99);
	history.push(std::move(invalid));
	CHECK_FALSE(history.can_undo());

	UndoStep mixed;
	mixed.canvas_transform = ImageTransform::Rotate180;
	mixed.patches.push_back(PixelPatch{
		Rect{0, 0, 1, 1}, {0}, {1}});
	history.push(std::move(mixed));
	CHECK_FALSE(history.can_undo());
}

TEST_CASE("session transform commits an active stroke first")
{
	PaintSession session(4, 4);
	session.clear_history();

	StrokeProbeTool tool;
	session.set_tool(&tool);
	session.handle_mouse_press({0, 0}, ToolBase::kLeftButton);
	session.handle_mouse_move({1, 1});
	CHECK(tool.is_drawing());

	REQUIRE(session.transform_image(ImageTransform::Rotate90));
	CHECK_FALSE(tool.is_drawing());
	CHECK_EQ(session.getWidth(), 4);
	CHECK_EQ(session.getHeight(), 4);
	// The stroke pixel at (1,1) rotates clockwise to (2,1).
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 1), 1);

	REQUIRE(session.undo());
	CHECK_EQ(session.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 1), 0);
	REQUIRE(session.undo());
	CHECK_EQ(session.get_main_buffer().get_pixel(1, 1), 0);
	CHECK_FALSE(session.can_undo());

	REQUIRE(session.redo());
	REQUIRE(session.redo());
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 1), 1);
}

TEST_CASE("session transform accepts a floating selection first")
{
	PaintSession session(4, 4);
	REQUIRE(session.document().apply_pixel_patch(
		Rect{0, 0, 4, 4},
		{1, 2, 0, 0,
		 0, 0, 0, 0,
		 0, 0, 0, 0,
		 0, 0, 0, 0}));
	session.clear_history();
	const std::uint64_t initial_state = session.current_state_id();

	REQUIRE(session.begin_selection(Rect{0, 0, 2, 2}, 0));
	REQUIRE(session.move_floating_region(1, 1));
	REQUIRE(session.has_floating_region());

	REQUIRE(session.transform_image(ImageTransform::Rotate90));
	CHECK_FALSE(session.has_floating_region());
	CHECK_EQ(session.getWidth(), 4);
	CHECK_EQ(session.getHeight(), 4);

	// After the selection commit and the transform, the pixels are:
	// row0 all 0; row1: x2=1; row2: x2=2.
	const ImageBuffer& transformed = session.get_main_buffer();
	CHECK_EQ(transformed.get_pixel(2, 1), 1);
	CHECK_EQ(transformed.get_pixel(2, 2), 2);
	const std::uint64_t transformed_state = session.current_state_id();
	CHECK(transformed_state != initial_state);

	REQUIRE(session.undo());
	const std::uint64_t after_first_undo = session.current_state_id();
	CHECK(after_first_undo != initial_state);
	CHECK(after_first_undo != transformed_state);
	CHECK_EQ(session.get_main_buffer().get_pixel(1, 1), 1);
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 1), 2);
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 2), 0);

	REQUIRE(session.undo());
	CHECK_EQ(session.current_state_id(), initial_state);
	CHECK_EQ(session.get_main_buffer().get_pixel(0, 0), 1);
	CHECK_EQ(session.get_main_buffer().get_pixel(1, 0), 2);
	CHECK_FALSE(session.can_undo());

	REQUIRE(session.redo());
	REQUIRE(session.redo());
	CHECK_EQ(session.current_state_id(), transformed_state);
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 1), 1);
	CHECK_EQ(session.get_main_buffer().get_pixel(2, 2), 2);
}
