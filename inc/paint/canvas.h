#ifndef QTREGBPAINT_CANVAS_H
#define QTREGBPAINT_CANVAS_H

#include "imgui.h"
#include "paint/image_buffer.h"

enum class Tool { Pencil };

class Canvas
{
	public:
		Canvas(int w = 256, int h = 256);
		
		void setTool(Tool t);
		void handleInput(const ImVec2& mousePos, bool isActive, const ImVec2& canvasPos, float zoom);
		void renderBuffer(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& scroll, float zoom, const ImVec2& canvasSize); 

		int getWidth() const;
		int getHeight() const;

	private:
		int width;
		int height;
		ImageBuffer buffer;
		Tool currentTool = Tool::Pencil;
		uint8_t m_foreground_color_index = 0;
		uint8_t m_background_color_index = 0;
};

#endif