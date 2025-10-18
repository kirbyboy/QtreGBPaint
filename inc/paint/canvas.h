#ifndef QTREGBPAINT_CANVAS_H
#define QTREGBPAINT_CANVAS_H

#include <memory>
#include "imgui.h"
#include "paint/image_buffer.h"
#include "paint/draw_buffer.h"
#include "core/color.h"
#include "tools/draw_tool.h"
#include "core/vec2.h"

class Canvas
{
	public:
		Canvas(int w = 256, int h = 256);
		
		void set_tool(std::unique_ptr<class DrawTool> tool);
		void handleInput(const ImVec2& mousePos, bool isActive, const ImVec2& canvasPos, float zoom);
		void renderBuffer(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& scroll, float zoom, const ImVec2& canvasSize); 

		void mousePressEvent(const PaintCore::Vec2& pos);
		void mouseMoveEvent(const PaintCore::Vec2& pos);
		void mouseReleaseEvent(const PaintCore::Vec2& pos);

		int getWidth() const;
		int getHeight() const;

		void update();
		const ImageBuffer& get_main_buffer() const;
		ImageBuffer& get_main_buffer();
	private:
		int m_width;
		int m_height;
		ImageBuffer m_buffer;
		DrawBuffer m_main_buffer;
		std::unique_ptr<DrawTool> m_current_tool;
		PaintCore::ColorIndex m_foreground_color_index = 0;
		PaintCore::ColorIndex m_background_color_index = 0;
};

#endif