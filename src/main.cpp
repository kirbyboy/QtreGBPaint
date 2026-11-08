#include <SDL.h>
#include <cstdio>
#include <memory>
#include <string>

#include "core/ibackend.h"
#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"
#include "paint/paint_frontend.h"
#include "tools/selection_tool.h"
#include "tools/square_tool.h"

int main()
{
	ImGuiManager imgui_manager;
	imgui_manager.init();

	SDLRendererBackend backend;
	backend.init();

	// Robust asset path (relative to the executable, not the cwd).
	std::string assetBase = "assets";
	if (char* basePath = SDL_GetBasePath())
	{
		assetBase = std::string(basePath) + "../assets";
		SDL_free(basePath);
	}

	PaintBackend resources(&backend);
	const Palette& grayPalette = resources.iconPalette();
	resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                    PaintCore::CursorType::Crosshair,
	                    assetBase + "/selection_normal.idx",
	                    assetBase + "/selection_hover.idx",
	                    assetBase + "/selection_pressed.idx", grayPalette);

	resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                    PaintCore::CursorType::Rect,
	                    assetBase + "/rect_normal.idx",
	                    assetBase + "/rect_hover.idx",
	                    assetBase + "/rect_pressed.idx", grayPalette);

	resources.addButton("Pencil", nullptr,
	                    PaintCore::CursorType::Pencil,
	                    assetBase + "/pencil_normal.idx",
	                    assetBase + "/pencil_hover.idx",
	                    assetBase + "/pencil_pressed.idx", grayPalette);

	resources.addButton("Fill", nullptr,
	                    PaintCore::CursorType::Fill,
	                    assetBase + "/fill_normal.idx",
	                    assetBase + "/fill_hover.idx",
	                    assetBase + "/fill_pressed.idx", grayPalette);

	resources.addButton("Circle", nullptr,
	                    PaintCore::CursorType::Circle,
	                    assetBase + "/circle_normal.idx",
	                    assetBase + "/circle_hover.idx",
	                    assetBase + "/circle_pressed.idx", grayPalette);

	const auto load_cursor = [&](const char* name, const std::string& path)
	{
		std::string error;
		if (!resources.loadCursor(name, path, error))
		{
			std::fprintf(stderr, "Cursor for %s: %s (system fallback)\n",
			             name, error.c_str());
		}
	};

	load_cursor("Rect", assetBase + "/cursors/rect.ipx");
	load_cursor("Pencil", assetBase + "/cursors/pencil.ipx");
	load_cursor("Fill", assetBase + "/cursors/fill.ipx");
	load_cursor("Circle", assetBase + "/cursors/circle.ipx");

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

	load_cursor_type(PaintCore::CursorType::Move, "Move",
	                 assetBase + "/cursors/move.ipx");

	const auto load_option_icon = [&](const char* key, const std::string& path,
	                                  int y)
	{
		if (!resources.loadOptionIcon(key, path, 0, y, 60, 16))
		{
			std::fprintf(stderr, "Option icon %s could not be loaded\n", key);
		}
	};

	load_option_icon("rect.outline", assetBase + "/rect_options.idx", 0);
	load_option_icon("rect.fill", assetBase + "/rect_options.idx", 16);
	load_option_icon("rect.both", assetBase + "/rect_options.idx", 32);
	load_option_icon("selection.normal",
	                 assetBase + "/selection_options.idx", 0);
	load_option_icon("selection.transparent",
	                 assetBase + "/selection_options.idx", 16);

	PaintFrontend main_window(resources);

	bool running = true;

	while (running)
	{
		// Process SDL events (and forward them to ImGui).
		backend.poll_events(running);

		imgui_manager.begin_frame(backend);
		main_window.render();
		backend.render_imgui(imgui_manager.end_frame());
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