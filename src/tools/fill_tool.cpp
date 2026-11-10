#include "tools/fill_tool.h"

#include <algorithm>
#include <vector>

#include "core/fill.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"

FillTool::FillTool(PaintCore::ColorIndex foreground,
                   PaintCore::ColorIndex background)
	: DrawTool(foreground, background)
{
	add_option(ToolOption::choice(
		"mode", "Fill mode", static_cast<int>(Mode::Contiguous),
		{
			{static_cast<int>(Mode::Contiguous), "Contiguous", "Area",
			 "fill.contiguous", {}},
			{static_cast<int>(Mode::Global), "Global", "Global",
			 "fill.global", {}},
		}));
}

void FillTool::on_mouse_press(PaintSession& canvas,
                              const PaintCore::Vec2& pos, int button)
{
	DrawTool::on_mouse_press(canvas, pos, button);
	if (!m_drawing)
	{
		return;
	}

	fill_from(canvas, pos);

	// The bucket is a one-shot tool: commit the result now instead of waiting
	// for the release. Keeping the stroke open would redraw the whole filled
	// area with per-pixel quads every frame while the button is held.
	m_drawing = false;
	m_active_button = 0;
	canvas.commit_draw_buffer(m_preview_buffer);
}

void FillTool::draw_preview(DrawBuffer& buffer,
                            const PaintCore::Modifiers& modifiers)
{
	(void)buffer;
	(void)modifiers;
	// The fill is computed and committed in on_mouse_press().
}

FillTool::Mode FillTool::mode() const
{
	return static_cast<Mode>(option_value("mode"));
}

void FillTool::fill_from(PaintSession& canvas,
                         const PaintCore::Vec2& seed)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	if (seed.x < 0 || seed.y < 0 ||
	    seed.x >= image.width() || seed.y >= image.height())
	{
		return;
	}

	const PaintCore::ColorIndex target = image.get_pixel(seed.x, seed.y);
	const PaintCore::ColorIndex replacement = m_stroke_foreground;
	if (target == replacement)
	{
		return;
	}

	const auto color_at = [&image](int x, int y)
	{
		return image.get_pixel(x, y);
	};

	const std::vector<PaintCore::FillSpan> spans =
		mode() == Mode::Global
			? PaintCore::collect_global_fill_spans(
				image.width(), image.height(), target, replacement,
				color_at)
			: PaintCore::collect_flood_fill_spans(
				image.width(), image.height(), seed.x, seed.y,
				target, replacement, color_at);
	if (spans.empty())
	{
		return;
	}

	// Size the buffer from the final bounding box once, instead of expanding
	// it row by row. The runs can be anywhere inside the canvas.
	int min_x = spans.front().x0;
	int max_x = spans.front().x1;
	int min_y = spans.front().y;
	int max_y = spans.front().y;
	for (const PaintCore::FillSpan& span : spans)
	{
		min_x = std::min(min_x, span.x0);
		max_x = std::max(max_x, span.x1);
		min_y = std::min(min_y, span.y);
		max_y = std::max(max_y, span.y);
	}

	m_preview_buffer.ensure_fit(min_x, min_y);
	m_preview_buffer.ensure_fit(max_x, max_y);

	const int buffer_x = m_preview_buffer.x();
	const int buffer_y = m_preview_buffer.y();
	for (const PaintCore::FillSpan& span : spans)
	{
		const int y = span.y - buffer_y;
		for (int x = span.x0; x <= span.x1; ++x)
		{
			m_preview_buffer.set_pixel(x - buffer_x, y, replacement);
		}
	}
}
