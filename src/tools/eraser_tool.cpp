#include "tools/eraser_tool.h"

#include <utility>

namespace
{
ToolOptionChoice make_size_choice(int value, const char* label,
                                  const char* short_label, int size)
{
	ToolOptionChoice choice;
	choice.value = value;
	choice.label = label;
	choice.short_label = short_label;
	choice.preview = PaintCore::make_brush_offsets(
		PaintCore::BrushShape::Square, size);
	return choice;
}
}

EraserTool::EraserTool(PaintCore::ColorIndex foreground,
                       PaintCore::ColorIndex background)
	: FreehandTool(foreground, background),
	  m_erase_color(background),
	  m_erase_mask_color(foreground)
{
	ToolOption option = ToolOption::choice(
		"size", "Size", static_cast<int>(Size::Tiny),
		{
			make_size_choice(static_cast<int>(Size::Tiny),
			                 "Eraser 4", "4", 4),
			make_size_choice(static_cast<int>(Size::Small),
			                 "Eraser 6", "6", 6),
			make_size_choice(static_cast<int>(Size::Medium),
			                 "Eraser 8", "8", 8),
			make_size_choice(static_cast<int>(Size::Large),
			                 "Eraser 10", "10", 10),
		});
	option.grid_columns = 1;
	add_option(std::move(option));

	update_size(static_cast<int>(Size::Tiny));
}

void EraserTool::on_mouse_press(PaintSession& canvas,
                                const PaintCore::Vec2& pos, int button)
{
	// Freeze the colors for the whole stroke; palette changes while drawing
	// should not change what this eraser pass replaces.
	m_erase_color = m_background;
	m_erase_mask_color = m_foreground;
	FreehandTool::on_mouse_press(canvas, pos, button);
}

ToolBase::StampPreview EraserTool::stamp_preview(
	PaintCore::ColorIndex foreground,
	PaintCore::ColorIndex background) const
{
	(void)foreground;
	const PaintCore::ColorIndex color =
		m_drawing ? m_erase_color : background;
	return {&m_offsets, color, true, true};
}

void EraserTool::stamp(DrawBuffer& buffer, const ImageBuffer& image,
                       int x, int y)
{
	const bool foreground_only = (m_active_button == kRightButton);

	for (const PaintCore::Vec2& offset : m_offsets)
	{
		const int px = x + offset.x;
		const int py = y + offset.y;

		if (foreground_only)
		{
			if (px < 0 || py < 0 ||
			    px >= image.width() || py >= image.height() ||
			    image.get_pixel(px, py) != m_erase_mask_color)
			{
				continue;
			}
		}

		buffer.set_pixel(px - buffer.x(), py - buffer.y(), m_erase_color);
	}
}

int EraserTool::stamp_margin() const
{
	return m_margin;
}

void EraserTool::on_option_changed(const std::string& id, int value)
{
	if (id == "size")
	{
		update_size(value);
	}
}

void EraserTool::update_size(int value)
{
	int size = 10;
	switch (static_cast<Size>(value))
	{
		case Size::Tiny:
			size = 4;
			break;
		case Size::Small:
			size = 6;
			break;
		case Size::Medium:
			size = 8;
			break;
		case Size::Large:
			size = 10;
			break;
		case Size::Count:
			break;
	}

	m_offsets = PaintCore::make_brush_offsets(
		PaintCore::BrushShape::Square, size);
	m_margin = PaintCore::brush_margin(m_offsets);
}
