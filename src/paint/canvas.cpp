#include "paint/canvas.h"

Canvas::Canvas(int w, int h): 
m_width(w), 
m_height(h), 
m_buffer(w, h, 2)
{
	m_buffer.fill(0);
}

void Canvas::set_tool(std::unique_ptr<DrawTool> tool)
{
	if(m_currentTool)
	{
		m_currentTool->cancel();
	}

	m_currentTool = std::move(tool);
}

void Canvas::handleInput(const ImVec2 &mousePos, bool isActive, const ImVec2 &canvasPos, float zoom)
{
	if(!isActive || currentTool != Tool::Pencil)
	{
		return;
	}

	int x = static_cast<int>((mousePos.x - canvasPos.x) / zoom);
	int y = static_cast<int>((mousePos.y - canvasPos.y) / zoom);
	if(x >= 0 && x < m_width && y >= 0 && y < m_height)
	{
		m_buffer.setPixel(x, y, 3);
	}
}

void Canvas::renderBuffer(ImDrawList *drawList, const ImVec2 &canvasPos, const ImVec2 &scroll, float zoom, const ImVec2 &canvasSize)
{
	int startX = static_cast<int>(scroll.x / zoom);
	int startY = static_cast<int>(scroll.y / zoom);
	int endX   = startX + static_cast<int>(canvasSize.x / zoom) + 1;
	int endY   = startY + static_cast<int>(canvasSize.y / zoom) + 1;

	// Clamp
	startX = std::max(0, startX);
	startY = std::max(0, startY);
	endX   = std::min(m_width, endX);
	endY   = std::min(m_height, endY);

	// Draw visible pixels
	for (int y = startY; y < endY; ++y)
	{
		for (int x = startX; x < endX; ++x)
		{
			uint8_t color = m_buffer.getPixel(x, y);
			ImU32 col = (color == 0) ? IM_COL32(255,255,255,255) :
						(color == 3) ? IM_COL32(0,0,0,255) :
						IM_COL32(128,128,128,255);

			drawList->AddRectFilled(
				ImVec2(canvasPos.x + (x - startX)*zoom, canvasPos.y + (y - startY)*zoom),
				ImVec2(canvasPos.x + (x - startX + 1)*zoom, canvasPos.y + (y - startY + 1)*zoom),
				col
			);
		}
	}
}

void Canvas::mousePressEvent(const PaintCore::Vec2 &pos)
{
	if(m_currentTool)
	{
		m_currentTool->onMousePress(*this, pos);
	}
}

void Canvas::mouseMoveEvent(const PaintCore::Vec2 &pos)
{
	if(m_currentTool)
	{
		m_currentTool->onMouseMove(*this, pos);
	}
}

void Canvas::mouseReleaseEvent(const PaintCore::Vec2 &pos)
{
	if(m_currentTool)
	{
		m_currentTool->onMouseRelease(*this, pos);
	}
}

int Canvas::getWidth() const
{
	return m_width;
}

int Canvas::getHeight() const
{
	return m_height;
}
const ImageBuffer &Canvas::get_main_buffer() const
{
	return m_buffer;
}

ImageBuffer &Canvas::get_main_buffer()
{
	return m_buffer;
}
