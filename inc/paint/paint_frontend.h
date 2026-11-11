#ifndef QTREGBPAINT_PAINT_FRONTEND_H
#define QTREGBPAINT_PAINT_FRONTEND_H

#include <cstddef>
#include <string>

#include "imgui.h"
#include "core/color.h"
#include "paint/canvas_area.h"
#include "paint/checkerboard.h"
#include "paint/image_export.h"
#include "paint/indexed_image.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"
#include "paint/theme.h"

class PaintFrontend
{
	public:
		using Theme = AppTheme;

		PaintFrontend(PaintBackend& backend);
		~PaintFrontend();
		void select_tool(const std::string& name);
		// Opens a file passed on the command line before the first frame.
		void open_initial_file(const std::string& path);

		// PNG used by Help > About. The frontend loads it lazily on first use.
		void set_about_logo_path(const std::string& path)
		{
			m_about_logo_path = path;
		}

		// Applies one of ImGui's built-in color presets immediately. The
		// built-in presets only change colors, not layout metrics.
		void set_theme(Theme theme);
		Theme theme() const { return m_theme; }

		// View > Grid is a per-user preference (persisted in AppConfig).
		bool grid_enabled() const { return m_show_grid; }
		void set_grid_enabled(bool enabled) { m_show_grid = enabled; }

		// Last directory used by Open/Save As (persisted in AppConfig).
		const std::string& last_directory() const { return m_last_directory; }
		void set_last_directory(const std::string& directory)
		{
			m_last_directory = directory;
		}

		bool render(); // Returns false when the application should close

	private:
		enum class FileAction
		{
			None,
			New,
			Open,
			OpenPath,
			Quit,
		};

		enum class FlipRotateTarget
		{
			Selection,
			Canvas,
		};

		void apply_theme_style();
		void renderMenuBar();
		void renderDialogs();
		void renderToolbar();
		void renderToolOptions();
		void renderPalette();
		void renderCanvas();
		void renderStatusBar();
		void renderFileDialogs();
		void draw_checkerboard(ImDrawList* draw_list, const ImVec2& min,
		                       const ImVec2& max, const ImVec2& origin);
		void ensure_about_logo();
		std::string file_dialog_directory() const;
		void remember_file_directory(const std::string& path);

		void request_new_document();
		void request_open_document();
		void request_open_path(std::string path);
		void open_recent_path(std::string path);
		void request_save();
		void request_save_as();
		void request_export(ImageExportFormat format);
		void export_to_path(const std::string& path, ImageExportFormat format);
		void save_to_path(const std::string& path, bool continue_pending);
		void save_to_path(const std::string& path, bool continue_pending,
		                  IndexedFormat format);
		void request_close_window();
		void copy_selection();
		void cut_selection();
		void paste_selection();
		void select_all();
		void open_properties_dialog();
		void open_flip_rotate_dialog(FlipRotateTarget target);
		void apply_flip_rotate();
		void request_zoom_steps(int steps);
		void apply_zoom_request();
		void perform_pending_file_action();
		void open_open_dialog();
		void open_save_as_dialog();
		void show_file_error(const std::string& message);

	private:
		PaintBackend& m_backend;
		CanvasArea m_canvas_area;
		PaintUi::CheckerboardTexture m_checkerboard;
		std::string m_about_logo_path;
		void* m_about_logo_texture = nullptr;
		int m_about_logo_width = 0;
		int m_about_logo_height = 0;
		bool m_about_logo_attempted = false;
		Theme m_theme = Theme::Light;
		bool m_theme_applied = false;
		int m_zoom = 1;
		int m_last_canvas_zoom = 1;
		int m_zoom_key_steps = 0;
		int m_zoom_wheel_steps = 0;
		float m_zoom_wheel_accum = 0.0f;
		ImVec2 m_zoom_wheel_anchor{};
		int m_zoom_slider_target = -1;
		ImVec2 m_pending_canvas_scroll{};
		bool m_apply_pending_canvas_scroll = false;
		bool m_show_grid = true;
		std::string m_last_directory;
		bool m_open_properties_popup = false;
		bool m_open_flip_rotate_popup = false;
		bool m_open_about_popup = false;
		FlipRotateTarget m_flip_rotate_target = FlipRotateTarget::Selection;
		int m_flip_rotate_mode = 0;
		int m_flip_rotate_angle = 90;
		int m_canvas_size_width = 0;
		int m_canvas_size_height = 0;
		int m_canvas_size_bpp = 2;

		FileAction m_pending_file_action = FileAction::None;
		std::string m_pending_open_path;
		bool m_open_unsaved_popup = false;
		bool m_open_new_popup = false;
		bool m_open_error_popup = false;
		bool m_save_then_pending = false;
		IndexedFormat m_save_format = IndexedFormat::Index;
		std::string m_pending_save_path;
		bool m_open_idx_options_popup = false;
		bool m_should_close = false;
		int m_new_width = 256;
		int m_new_height = 256;
		int m_new_bpp = 2;
		std::string m_error_message;
};

#endif
