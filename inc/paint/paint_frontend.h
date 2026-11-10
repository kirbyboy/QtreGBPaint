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
		enum class Theme
		{
			Dark,
			Light,
			Classic,
		};

		PaintFrontend(PaintBackend& backend);
		void select_tool(const std::string& name);

		// Applies one of ImGui's built-in color presets immediately. The
		// built-in presets only change colors, not layout metrics.
		void set_theme(Theme theme);
		Theme theme() const { return m_theme; }

		void render(); // Main render method

	private:
		enum class FileAction
		{
			None,
			New,
			Open,
		};

		void apply_theme_style();
		void renderMenuBar();
		void renderToolbar();
		void renderToolOptions();
		void renderPalette();
		void renderCanvas();
		void renderFileDialogs();

		void request_new_document();
		void request_open_document();
		void request_save();
		void request_save_as();
		void perform_pending_file_action();
		void open_open_dialog();
		void open_save_as_dialog();
		void save_to_path(const std::string& path, bool continue_pending);
		void show_file_error(const std::string& message);

	private:
		PaintBackend& m_backend;
		CanvasArea m_canvas_area;
		Theme m_theme = Theme::Dark;
		bool m_theme_applied = false;
		int m_zoom = 1;
		int m_last_canvas_zoom = 1;
		bool m_show_grid = true;
		bool m_open_canvas_size_popup = false;
		int m_canvas_size_width = 0;
		int m_canvas_size_height = 0;

		FileAction m_pending_file_action = FileAction::None;
		bool m_open_unsaved_popup = false;
		bool m_open_new_popup = false;
		bool m_open_error_popup = false;
		bool m_save_then_pending = false;
		int m_new_width = 256;
		int m_new_height = 256;
		int m_new_bpp = 2;
		std::string m_error_message;
};

#endif
