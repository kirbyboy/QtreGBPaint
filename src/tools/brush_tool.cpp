#include "tools/brush_tool.h"

#include <cstdlib>
#include <utility>

namespace
{
ToolOptionChoice make_brush_choice(int value, const char* label,
                                   const char* short_label,
                                   PaintCore::BrushShape shape, int size)
{
	ToolOptionChoice choice;
	choice.value = value;
	choice.label = label;
	choice.short_label = short_label;
	choice.preview = PaintCore::make_brush_offsets(shape, size);
	return choice;
}
}

BrushTool::BrushTool(PaintCore::ColorIndex foreground,
                     PaintCore::ColorIndex background)
	: FreehandTool(foreground, background)
{
	using PaintCore::BrushShape;

	// Circle option labels use the radius; the footprint size is 2*r+1.
	ToolOption option = ToolOption::choice(
		"brush", "Brush", static_cast<int>(Brush::CircleLarge),
		{
			make_brush_choice(static_cast<int>(Brush::CircleLarge),
			                  "Circle r3", "C3", BrushShape::Circle, 7),
			make_brush_choice(static_cast<int>(Brush::CircleMedium),
			                  "Circle r2", "C2", BrushShape::Circle, 5),
			make_brush_choice(static_cast<int>(Brush::CircleSmall),
			                  "Circle r1", "C1", BrushShape::Circle, 3),
			make_brush_choice(static_cast<int>(Brush::SquareLarge),
			                  "Square 8", "S8", BrushShape::Square, 8),
			make_brush_choice(static_cast<int>(Brush::SquareMedium),
			                  "Square 5", "S5", BrushShape::Square, 5),
			make_brush_choice(static_cast<int>(Brush::SquareSmall),
			                  "Square 2", "S2", BrushShape::Square, 2),
			make_brush_choice(static_cast<int>(Brush::DiagonalDownLarge),
			                  "Diagonal 45 8", "D8",
			                  BrushShape::DiagonalDown, 8),
			make_brush_choice(static_cast<int>(Brush::DiagonalDownMedium),
			                  "Diagonal 45 5", "D5",
			                  BrushShape::DiagonalDown, 5),
			make_brush_choice(static_cast<int>(Brush::DiagonalDownSmall),
			                  "Diagonal 45 2", "D2",
			                  BrushShape::DiagonalDown, 2),
			make_brush_choice(static_cast<int>(Brush::DiagonalUpLarge),
			                  "Diagonal 135 8", "I8",
			                  BrushShape::DiagonalUp, 8),
			make_brush_choice(static_cast<int>(Brush::DiagonalUpMedium),
			                  "Diagonal 135 5", "I5",
			                  BrushShape::DiagonalUp, 5),
			make_brush_choice(static_cast<int>(Brush::DiagonalUpSmall),
			                  "Diagonal 135 2", "I2",
			                  BrushShape::DiagonalUp, 2),
		});
	option.grid_columns = 3;
	add_option(std::move(option));

	update_brush(static_cast<int>(Brush::CircleLarge));
}

ToolBase::StampPreview BrushTool::stamp_preview(
	PaintCore::ColorIndex foreground,
	PaintCore::ColorIndex background) const
{
	(void)background;
	const PaintCore::ColorIndex color =
		m_drawing ? m_stroke_foreground : foreground;
	return {&m_offsets, color, false, false};
}

void BrushTool::stamp(DrawBuffer& buffer, const ImageBuffer& image,
                      int x, int y)
{
	(void)image;
	for (const PaintCore::Vec2& offset : m_offsets)
	{
		buffer.set_pixel(x + offset.x - buffer.x(),
		                 y + offset.y - buffer.y(),
		                 m_stroke_foreground);
	}
}

int BrushTool::stamp_margin() const
{
	return m_margin;
}

void BrushTool::stamp_segment_step(DrawBuffer& buffer,
                                   const ImageBuffer& image,
                                   const PaintCore::Vec2& from,
                                   const PaintCore::Vec2& to)
{
	const int dx = to.x - from.x;
	const int dy = to.y - from.y;
	if (std::abs(dx) != 1 || std::abs(dy) != 1)
	{
		return;
	}

	const bool path_down = (dx == dy);
	const bool brush_down =
		(m_shape == PaintCore::BrushShape::DiagonalDown);
	const bool brush_up =
		(m_shape == PaintCore::BrushShape::DiagonalUp);

	// A 1-pixel diagonal brush leaves a one-pixel gap when the pointer moves
	// along the opposite perfect diagonal. Stamp the intermediate corner to
	// fill the swept parallelogram; parallel movement needs no extra stamp.
	if ((brush_down && !path_down) || (brush_up && path_down))
	{
		stamp(buffer, image, to.x, from.y);
	}
}

void BrushTool::on_option_changed(const std::string& id, int value)
{
	if (id == "brush")
	{
		update_brush(value);
	}
}

void BrushTool::update_brush(int value)
{
	using PaintCore::BrushShape;

	BrushShape shape = BrushShape::Circle;
	int size = 3;

	switch (static_cast<Brush>(value))
	{
		case Brush::CircleLarge:
			shape = BrushShape::Circle;
			size = 7;
			break;
		case Brush::CircleMedium:
			shape = BrushShape::Circle;
			size = 5;
			break;
		case Brush::CircleSmall:
			shape = BrushShape::Circle;
			size = 3;
			break;
		case Brush::SquareLarge:
			shape = BrushShape::Square;
			size = 8;
			break;
		case Brush::SquareMedium:
			shape = BrushShape::Square;
			size = 5;
			break;
		case Brush::SquareSmall:
			shape = BrushShape::Square;
			size = 2;
			break;
		case Brush::DiagonalDownLarge:
			shape = BrushShape::DiagonalDown;
			size = 8;
			break;
		case Brush::DiagonalDownMedium:
			shape = BrushShape::DiagonalDown;
			size = 5;
			break;
		case Brush::DiagonalDownSmall:
			shape = BrushShape::DiagonalDown;
			size = 2;
			break;
		case Brush::DiagonalUpLarge:
			shape = BrushShape::DiagonalUp;
			size = 8;
			break;
		case Brush::DiagonalUpMedium:
			shape = BrushShape::DiagonalUp;
			size = 5;
			break;
		case Brush::DiagonalUpSmall:
			shape = BrushShape::DiagonalUp;
			size = 2;
			break;
		case Brush::Count:
			break;
	}

	m_offsets = PaintCore::make_brush_offsets(shape, size);
	m_margin = PaintCore::brush_margin(m_offsets);
	m_shape = shape;
}
