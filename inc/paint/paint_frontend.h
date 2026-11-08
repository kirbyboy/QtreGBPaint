#ifndef QTREGBPAINT_PAINT_FRONTEND_H
#define QTREGBPAINT_PAINT_FRONTEND_H

#include <cstddef>
#include <string>

#include "imgui.h"
#include "core/color.h"
#include "paint/canvas_area.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"

class PaintFrontend
{
	public:
		PaintFrontend(PaintBackend& backend);
		void select_tool(const std::string& name);
		void render(); // Main render method

	private:
		enum class Gradient
		{
			Red,
			Green,
			Blue,
		};

		void renderMenuBar();
		void renderToolbar();
		void renderToolOptions();
		void renderPalette();
		void renderCanvas();
		void regenerate_palette();
		void apply_tool_colors();

	private:
		PaintBackend& m_backend;
		CanvasArea m_canvas_area;
		int m_zoom = 1;
		int m_last_canvas_zoom = 1;
		bool m_show_grid = true;
		bool m_open_canvas_size_popup = false;
		int m_canvas_size_width = 0;
		int m_canvas_size_height = 0;

		// Single palette used both by the panel and by the canvas. It always
		// has room for 32 entries; m_palette_color_count is the number of
		// visible/usable colors.
		Palette m_palette{5};
		int m_palette_color_count = 4;
		Gradient m_gradient = Gradient::Red;
		bool m_use_dmg_palette = true;
		std::size_t m_foreground_index = 3;
		std::size_t m_background_index = 0;
};

#endif