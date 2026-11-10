#include "tools/pick_color_tool.h"

#include "paint/image_buffer.h"
#include "paint/paint_session.h"

void PickColorTool::on_mouse_press(PaintSession& canvas,
                                   const PaintCore::Vec2& pos, int button)
{
	if (button != kLeftButton && button != kRightButton)
	{
		return;
	}

	const ImageBuffer& image = canvas.get_main_buffer();
	if (pos.x < 0 || pos.y < 0 ||
	    pos.x >= image.width() || pos.y >= image.height())
	{
		return;
	}

	const PaintCore::ColorIndex color = image.get_pixel(pos.x, pos.y);
	if (color == PaintCore::TRANSPARENT_INDEX)
	{
		return;
	}

	m_pick.color = color;
	m_pick.background = (button == kRightButton);
	m_has_pick = true;
}

void PickColorTool::on_mouse_move(PaintSession& canvas,
                                  const PaintCore::Vec2& pos)
{
	(void)canvas;
	(void)pos;
}

void PickColorTool::on_mouse_release(PaintSession& canvas,
                                     const PaintCore::Vec2& pos, int button)
{
	(void)canvas;
	(void)pos;
	(void)button;
}

void PickColorTool::on_cancel(PaintSession& canvas)
{
	(void)canvas;
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
