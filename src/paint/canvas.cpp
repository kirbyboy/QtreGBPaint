#include "paint/canvas.h"
#include "tools/draw_tool.h"
#include "core/util.h"
#include "paint/canvas_renderer.h"
#include <algorithm>

using namespace PaintCore;
using namespace std;

Canvas::Canvas(int w, int h): 
m_image_buffer(w, h, 2)
{
	m_image_buffer.fill(0);
}

int Canvas::getWidth() const
{
	return m_image_buffer.width;
}

int Canvas::getHeight() const
{
	return m_image_buffer.height;
}

void Canvas::render(ICanvasRenderer& renderer, const Viewport& view, const Palette& palette)
{
	render_image_buffer(renderer, view, palette);
    render_draw_buffer(renderer, view, palette);
}

void Canvas::render_image_buffer(ICanvasRenderer &renderer, const Viewport &view, const Palette &palette)
{
	int startX = static_cast<int>(view.scroll_x);
	int startY = static_cast<int>(view.scroll_y);

	int visibleW = (view.zoom > 0.0f) ? static_cast<int>(view.canvas_width_px / view.zoom) : 0;
	int visibleH = (view.zoom > 0.0f) ? static_cast<int>(view.canvas_height_px / view.zoom) : 0;

	int endX = min(m_image_buffer.width, startX + visibleW);
    int endY = min(m_image_buffer.height, startY + visibleH);

	for (int y = startY; y < endY; ++y)
	{
		for (int x = startX; x < endX; ++x)
		{
			ColorIndex c = m_image_buffer.data[y * m_image_buffer.width + x];
			if(c == PaintCore::TRANSPARENT_INDEX)	// It should never enter here...
			{
				continue;
			}
			// Coordenadas de IMAGEN; el renderer aplica zoom/canvas_pos
            renderer.draw_rect_filled(x, y, 1, 1, palette.getColor(c));
		}
	}
}

void Canvas::render_draw_buffer(ICanvasRenderer &renderer, const Viewport &view, const Palette &palette)
{
	if(!m_active_tool)
	{
		return;
	}
	
	auto draw_buffer = m_active_tool->get_draw_buffer();
	if(!draw_buffer || !draw_buffer->is_active())
	{
		return;
	}

	int buf_x = draw_buffer->x();
	int buf_y = draw_buffer->y();

    int buf_w = draw_buffer->width();
    int buf_h = draw_buffer->height();

	const vector<uint8_t>& data = draw_buffer->data();

	for (int y = 0; y < buf_h; ++y)
	{
		for (int x = 0; x < buf_w; ++x)
		{
			uint8_t color_index = data[y * buf_w + x];
			if(color_index == PaintCore::TRANSPARENT_INDEX)
			{
				continue;
			}

			// Coordenadas de imagen del preview (buf origin + offset)
            renderer.draw_rect_filled(buf_x + x, buf_y + y, 1, 1, palette.getColor(color_index));
		}
	}	
}

void Canvas::handle_mouse_press(const Vec2 &pos, int button)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_press(*this, pos, button);
	}
}

void Canvas::handle_mouse_move(const Vec2 &pos)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_move(*this, pos);
	}
}

void Canvas::handle_mouse_release(const Vec2 &pos, int button)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_release(*this, pos, button);
	}
}

void Canvas::set_tool(shared_ptr<ToolBase> tool)
{
	if(m_active_tool)
	{
		m_active_tool->on_cancel(*this);
	}

	m_active_tool = tool;
}

ToolBase* Canvas::active_tool() const
{
	return m_active_tool.get();
}

const ImageBuffer &Canvas::get_main_buffer() const
{
	return m_image_buffer;
}

ImageBuffer &Canvas::get_main_buffer()
{
	return m_image_buffer;
}