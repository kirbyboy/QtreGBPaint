#include "tools/pick_color_tool.h"

#include "paint/image_buffer.h"
#include "paint/paint_session.h"

namespace
{
bool pixel_in_image(const ImageBuffer& image, const PaintCore::Vec2& pos)
{
	return pos.x >= 0 && pos.y >= 0 &&
	       pos.x < image.width() && pos.y < image.height();
}
} // namespace

void PickColorTool::on_mouse_press(PaintSession& canvas,
                                   const PaintCore::Vec2& pos, int button)
{
	if (m_pressed_button != 0 ||
	    (button != kLeftButton && button != kRightButton))
	{
		return;
	}

	m_pressed_button = button;
	m_has_pick = false;
	m_has_preview = false;
	update_preview(canvas, pos);
}

void PickColorTool::on_mouse_move(PaintSession& canvas,
                                  const PaintCore::Vec2& pos)
{
	if (m_pressed_button == 0)
	{
		return;
	}

	update_preview(canvas, pos);
}

void PickColorTool::on_mouse_release(PaintSession& canvas,
                                     const PaintCore::Vec2& pos, int button)
{
	if (m_pressed_button == 0 || button != m_pressed_button)
	{
		return;
	}

	m_pressed_button = 0;
	m_has_preview = false;

	// The sample happens at the release position. Releasing outside the image
	// keeps the current palette colors. Any index can be picked, including one
	// that the palette currently renders as transparent.
	const ImageBuffer& image = canvas.get_main_buffer();
	if (!pixel_in_image(image, pos))
	{
		return;
	}

	const PaintCore::ColorIndex color = image.get_pixel(pos.x, pos.y);
	m_pick.color = color;
	m_pick.background = (button == kRightButton);
	m_has_pick = true;
}

void PickColorTool::on_cancel(PaintSession& canvas)
{
	(void)canvas;
	clear_press();
	m_has_pick = false;
}

bool PickColorTool::take_color_pick(ColorPick& pick)
{
	if (!m_has_pick)
	{
		return false;
	}

	pick = m_pick;
	m_has_pick = false;
	return true;
}

bool PickColorTool::color_pick_preview(PaintCore::ColorIndex& color) const
{
	if (m_pressed_button == 0 || !m_has_preview)
	{
		return false;
	}

	color = m_preview_color;
	return true;
}

void PickColorTool::update_preview(PaintSession& canvas,
                                   const PaintCore::Vec2& pos)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	if (!pixel_in_image(image, pos))
	{
		// Keep the last valid preview while the pointer is outside the image.
		return;
	}

	const PaintCore::ColorIndex color = image.get_pixel(pos.x, pos.y);
	m_preview_color = color;
	m_has_preview = true;
}

void PickColorTool::clear_press()
{
	m_pressed_button = 0;
	m_has_preview = false;
}
