#include "paint/canvas.h"
#include "tools/draw_tool.h"
#include "paint/canvas_renderer.h"
#include "paint/draw_buffer.h"
#include <algorithm>
#include <cmath>

using namespace PaintCore;
using namespace std;

Canvas::Canvas(int w, int h, HistoryLimits history_limits):
m_image_buffer(w, h, 2),
m_history(history_limits)
{
	m_image_buffer.fill(0);
}

int Canvas::getWidth() const
{
	return m_image_buffer.width();
}

int Canvas::getHeight() const
{
	return m_image_buffer.height();
}

void Canvas::render(ICanvasRenderer& renderer, const ViewTransform& view, const Palette& palette)
{
	render_image_buffer(renderer, view, palette);
	render_preview(renderer, palette);
}

void Canvas::render_image_buffer(ICanvasRenderer &renderer, const ViewTransform &view, const Palette &palette)
{
	const Rect visible = view.visible_image_rect(m_image_buffer.width(), m_image_buffer.height());

	for (int y = visible.y; y < visible.bottom(); ++y)
	{
		for (int x = visible.x; x < visible.right(); ++x)
		{
			ColorIndex c = m_image_buffer.data()[
				static_cast<std::size_t>(y) * static_cast<std::size_t>(m_image_buffer.width()) +
				static_cast<std::size_t>(x)];
			if(c == PaintCore::TRANSPARENT_INDEX)	// It should never enter here...
			{
				continue;
			}
			// Coordenadas de IMAGEN; el renderer aplica la transformación
			renderer.draw_rect_filled(x, y, 1, 1, palette.getColor(c));
		}
	}
}

void Canvas::render_preview(ICanvasRenderer &renderer, const Palette &palette)
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

void Canvas::commit_draw_buffer(DrawBuffer& buffer)
{
	if (!buffer.is_active())
	{
		buffer.reset();
		return;
	}

	const int x0 = std::max(0, buffer.x());
	const int y0 = std::max(0, buffer.y());
	const int x1 = std::min(m_image_buffer.width(), buffer.x() + buffer.width());
	const int y1 = std::min(m_image_buffer.height(), buffer.y() + buffer.height());

	if (x0 >= x1 || y0 >= y1)
	{
		buffer.reset();
		return;
	}

	const PaintCore::Rect rect{x0, y0, x1 - x0, y1 - y0};
	std::vector<PaintCore::ColorIndex> before = m_image_buffer.read_region(rect);

	buffer.blend_onto(m_image_buffer);
	buffer.reset();

	std::vector<PaintCore::ColorIndex> after = m_image_buffer.read_region(rect);
	if (before == after)
	{
		return;
	}

	UndoStep step;
	step.patches.push_back(PixelPatch{rect, std::move(before), std::move(after)});
	m_history.push(std::move(step));
}

bool Canvas::can_undo() const
{
	return m_history.can_undo();
}

bool Canvas::can_redo() const
{
	return m_history.can_redo();
}

bool Canvas::undo()
{
	// Un trazo a medias todavía no está en el historial: deshacerlo
	// equivale a cancelarlo.
	if (m_active_tool && m_active_tool->is_drawing())
	{
		m_active_tool->on_cancel(*this);
		return true;
	}

	return m_history.undo(m_image_buffer);
}

bool Canvas::redo()
{
	if (m_active_tool && m_active_tool->is_drawing())
	{
		return false;
	}

	return m_history.redo(m_image_buffer);
}

void Canvas::clear_history()
{
	m_history.clear();
}

void Canvas::set_history_limits(HistoryLimits limits)
{
	m_history.set_limits(limits);
}

HistoryLimits Canvas::history_limits() const
{
	return m_history.limits();
}

void Canvas::set_tool(ToolBase* tool)
{
	if(m_active_tool)
	{
		m_active_tool->on_cancel(*this);
	}

	m_active_tool = tool;
}

ToolBase* Canvas::active_tool() const
{
	return m_active_tool;
}

const ImageBuffer &Canvas::get_main_buffer() const
{
	return m_image_buffer;
}

ImageBuffer &Canvas::get_main_buffer()
{
	return m_image_buffer;
}