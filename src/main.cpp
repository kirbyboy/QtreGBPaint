#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "core/ibackend.h"
#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/app_config.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"
#include "paint/paint_frontend.h"
#include "paint/png_image.h"
#include "tools/brush_tool.h"
#include "tools/circle_tool.h"
#include "tools/curve_tool.h"
#include "tools/eraser_tool.h"
#include "tools/fill_tool.h"
#include "tools/line_tool.h"
#include "tools/pencil_tool.h"
#include "tools/pick_color_tool.h"
#include "tools/selection_tool.h"
#include "tools/square_tool.h"

namespace
{
bool asset_directory_has_logo(const std::string& directory)
{
	std::error_code error;
	return std::filesystem::is_regular_file(
		std::filesystem::path(directory) / "QtreGBPaint.png", error);
}

// Search the layouts used by the build tree, AppImage and a system install.
std::string resolve_asset_base()
{
	std::vector<std::string> candidates;
	if (char* base_path = SDL_GetBasePath())
	{
		const std::string base(base_path);
		SDL_free(base_path);
		candidates.push_back(base + "assets");
		candidates.push_back(base + "../assets");
		candidates.push_back(base + "../share/qtregbpaint/assets");
	}
	candidates.push_back("/usr/share/qtregbpaint/assets");
	candidates.push_back("/usr/local/share/qtregbpaint/assets");
	candidates.push_back("assets");

	for (const std::string& candidate : candidates)
	{
		if (asset_directory_has_logo(candidate))
		{
			return candidate;
		}
	}
	return "assets";
}
// Some X11 window managers reject very large _NET_WM_ICON properties, so the
// window icon is downscaled to a size that works everywhere. The About dialog
// still uses the full-resolution PNG.
PaintCore::RgbaImage make_window_icon(const PaintCore::RgbaImage& source,
                                      int max_size)
{
	if (source.width <= 0 || source.height <= 0 || source.pixels.empty() ||
	    max_size <= 0 ||
	    (source.width <= max_size && source.height <= max_size))
	{
		return source;
	}

	const int longest = std::max(source.width, source.height);
	const int width = std::max(1, source.width * max_size / longest);
	const int height = std::max(1, source.height * max_size / longest);
	PaintCore::RgbaImage icon;
	icon.width = width;
	icon.height = height;
	icon.pixels.resize(static_cast<std::size_t>(width) *
	                   static_cast<std::size_t>(height));
	for (int y = 0; y < height; ++y)
	{
		const int source_y = y * source.height / height;
		for (int x = 0; x < width; ++x)
		{
			const int source_x = x * source.width / width;
			icon.pixels[static_cast<std::size_t>(y) * width + x] =
				source.pixels[static_cast<std::size_t>(source_y) *
				                  source.width +
				              source_x];
		}
	}
	return icon;
}
} // namespace

int main(int argc, char** argv)
{
	const AppConfig config = AppConfig::load_default();

	ImGuiManager imgui_manager;
	imgui_manager.init();

	SDLRendererBackend backend;
	backend.init_with_geometry(config.window);

	// Robust asset path (relative to the executable, not the cwd).
	const std::string assetBase = resolve_asset_base();

	// Give the window a real icon. Wayland compositors normally use the
	// application id instead; both paths point to the same desktop entry.
	PaintCore::RgbaImage window_icon;
	std::string window_icon_error;
	if (PaintCore::load_png_rgba(assetBase + "/QtreGBPaint.png", window_icon,
	                             window_icon_error))
	{
		const PaintCore::RgbaImage icon = make_window_icon(window_icon, 64);
		backend.set_window_icon(icon.pixels.data(), icon.width, icon.height);
	}
	else
	{
		std::fprintf(stderr, "Window icon: %s\n", window_icon_error.c_str());
	}

	PaintBackend resources(&backend);
	resources.set_recent_files(config.recent_files);
	const Palette& grayPalette = resources.iconPalette();
	resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/selection_button.idx", grayPalette);

	resources.addButton("Eraser", std::make_shared<EraserTool>(1, 2),
	                    PaintCore::CursorType::Eraser,
	                    assetBase + "/eraser_button.idx", grayPalette);

	resources.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                    PaintCore::CursorType::Fill,
	                    assetBase + "/fill_button.idx", grayPalette);

	resources.addButton("Pick Color", std::make_shared<PickColorTool>(),
	                    PaintCore::CursorType::Pick,
	                    assetBase + "/pick_button.idx", grayPalette);

	resources.addButton("Pencil", std::make_shared<PencilTool>(1, 2),
	                    PaintCore::CursorType::Pencil,
	                    assetBase + "/pencil_button.idx", grayPalette);

	resources.addButton("Brush", std::make_shared<BrushTool>(1, 2),
	                    PaintCore::CursorType::Brush,
	                    assetBase + "/brush_button.idx", grayPalette);

	resources.addButton("Line", std::make_shared<LineTool>(1, 2),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/line_button.idx", grayPalette);

	resources.addButton("Curve", std::make_shared<CurveTool>(1, 2),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/curve_button.idx", grayPalette);

	resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/rect_button.idx", grayPalette);

	resources.addButton("Circle", std::make_shared<CircleTool>(1, 2),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/circle_button.idx", grayPalette);

	// Default editing tool for a fresh application.
	resources.selectButton("Pencil");

	const auto load_cursor = [&](const char* name, const std::string& path)
	{
		std::string error;
		if (!resources.loadCursor(name, path, error))
		{
			std::fprintf(stderr, "Cursor for %s: %s (system fallback)\n",
			             name, error.c_str());
		}
	};

	load_cursor("Pencil", assetBase + "/cursors/pencil.ipx");
	load_cursor("Brush", assetBase + "/cursors/slim_cross.ipx");
	load_cursor("Fill", assetBase + "/cursors/fill.ipx");
	load_cursor("Pick Color", assetBase + "/cursors/pick.ipx");

	if (!resources.setCursorImage(PaintCore::CursorType::Eraser,
	                              PaintCore::make_invisible_cursor()))
	{
		std::fprintf(stderr,
		             "Eraser cursor could not be created (system fallback)\n");
	}

	// Cursors that are not tied to a toolbar button (overrides like Move).
	const auto load_cursor_type = [&](PaintCore::CursorType type,
	                                  const char* name,
	                                  const std::string& path)
	{
		std::string error;
		if (!resources.loadCursor(type, path, error))
		{
			std::fprintf(stderr, "Cursor for %s: %s (system fallback)\n",
			             name, error.c_str());
		}
	};

	load_cursor_type(PaintCore::CursorType::Crosshair, "Crosshair",
	                 assetBase + "/cursors/cross.ipx");
	load_cursor_type(PaintCore::CursorType::Move, "Move",
	                 assetBase + "/cursors/move.ipx");

	const auto load_option_icon = [&](const char* key, const std::string& path,
	                                  int y, int height)
	{
		if (!resources.loadOptionIcon(key, path, 0, y, 60, height))
		{
			std::fprintf(stderr, "Option icon %s could not be loaded\n", key);
		}
	};

	load_option_icon("rect.outline", assetBase + "/rect_options.idx", 0, 16);
	load_option_icon("rect.fill", assetBase + "/rect_options.idx", 16, 16);
	load_option_icon("rect.both", assetBase + "/rect_options.idx", 32, 16);
	load_option_icon("selection.normal",
	                 assetBase + "/selection_options.idx", 0, 16);
	load_option_icon("selection.transparent",
	                 assetBase + "/selection_options.idx", 16, 16);
	load_option_icon("fill.contiguous",
	                 assetBase + "/fill_options.idx", 0, 46);
	load_option_icon("fill.global",
	                 assetBase + "/fill_options.idx", 46, 46);

	PaintFrontend main_window(resources);
	main_window.set_about_logo_path(assetBase + "/QtreGBPaint.png");
	main_window.set_theme(config.theme);
	main_window.set_grid_enabled(config.grid_enabled);
	main_window.set_last_directory(config.last_directory);
	if (argc > 1)
	{
		main_window.open_initial_file(argv[1]);
	}

	bool running = true;

	while (running)
	{
		// Process SDL events (and forward them to ImGui).
		backend.poll_events();

		imgui_manager.begin_frame(backend);
		running = main_window.render();
		backend.render_imgui(imgui_manager.end_frame());
	}

	// Persist preferences and window placement once per run. Settings are
	// intentionally not written every frame.
	AppConfig saved = config;
	saved.theme = main_window.theme();
	saved.grid_enabled = main_window.grid_enabled();
	saved.last_directory = main_window.last_directory();
	saved.window = backend.window_geometry();
	saved.recent_files = resources.recent_files();
	std::string config_error;
	if (!saved.save_default(config_error))
	{
		std::fprintf(stderr, "Could not save settings: %s\n",
		             config_error.c_str());
	}

	backend.shutdown();
	imgui_manager.shutdown();

	return 0;
}


/*
a) ImageBuffer
b) OverlayRegion
struct OverlayRegion {
    ImageBuffer buffer;
    int offsetX, offsetY;
};
c) Composition functions
void blitWithClipping(ImageBuffer& dst, const OverlayRegion& src);
void resizeOverlay(OverlayRegion& overlay, const Rect& newBounds);


*/