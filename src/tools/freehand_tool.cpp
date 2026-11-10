#include "tools/freehand_tool.h"

#include "core/line.h"

FreehandTool::FreehandTool(PaintCore::ColorIndex foreground,
                           PaintCore::ColorIndex background)
	: DrawTool(foreground, background)
{
}

void FreehandTool::on_mouse_press(PaintSession& canvas,
                                  const PaintCore::Vec2& pos, int button)
{
	DrawTool::on_mouse_press(canvas, pos, button);
	if (!m_drawing)
	{
		return;
	}

	m_last_pos = pos;
	append_segment(canvas, pos, pos);
}

void FreehandTool::on_mouse_move(PaintSession& canvas,
                                 const PaintCore::Vec2& pos)
{
	if (!m_drawing)
	{
		return;
	}

	m_current_pos = pos;
	append_segment(canvas, m_last_pos, pos);
	m_last_pos = pos;
}

void FreehandTool::on_mouse_release(PaintSession& canvas,
                                    const PaintCore::Vec2& pos, int button)
{
	if (!m_drawing)
	{
		return;
	}
	if (button != m_active_button)
	{
		return;
	}

	m_current_pos = pos;
	append_segment(canvas, m_last_pos, pos);
	m_drawing = false;
	m_active_button = 0;
	canvas.commit_draw_buffer(m_preview_buffer);
}

void FreehandTool::draw_preview(DrawBuffer& buffer,
                                const PaintCore::Modifiers& modifiers)
{
	(void)buffer;
	(void)modifiers;
}

void FreehandTool::append_segment(PaintSession& canvas,
                                  const PaintCore::Vec2& from,
                                  const PaintCore::Vec2& to)
{
	m_preview_buffer.set_bounds(canvas.getWidth(), canvas.getHeight());

	const int margin = stamp_margin();
	m_preview_buffer.ensure_fit(from.x - margin, from.y - margin);
	m_preview_buffer.ensure_fit(from.x + margin, from.y + margin);
	m_preview_buffer.ensure_fit(to.x - margin, to.y - margin);
	m_preview_buffer.ensure_fit(to.x + margin, to.y + margin);

	int previous_x = from.x;
	int previous_y = from.y;
	bool has_previous = false;
	const ImageBuffer& image = canvas.get_main_buffer();

	PaintCore::rasterize_line(
		from.x, from.y, to.x, to.y,
		[&](int x, int y)
		{
			if (has_previous)
			{
				stamp_segment_step(
					m_preview_buffer, image,
					PaintCore::Vec2{previous_x, previous_y},
					PaintCore::Vec2{x, y});
			}
			stamp(m_preview_buffer, image, x, y);
			previous_x = x;
			previous_y = y;
			has_previous = true;
		});
}
