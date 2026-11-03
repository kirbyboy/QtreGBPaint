#include <SDL.h>
#include <memory>
#include <unistd.h>
#include <limits.h>

#include "core/ibackend.h"
#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"
#include "paint/paint_frontend.h"
#include "tools/square_tool.h"

int main()
{
	ImGuiManager imgui_manager;
	imgui_manager.init();

	SDLRendererBackend backend;
	backend.init();

	const Palette grayPalette = Palette::grayscale(3);

	// Ruta robusta a los assets (relativa al ejecutable, no al cwd)
	char exeBuf[PATH_MAX];
	std::string assetBase = "assets";
	ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
	if (len != -1)
	{
		exeBuf[len] = '\0';
		std::string exeDir = exeBuf;
		auto pos = exeDir.find_last_of("/\\");
		if (pos != std::string::npos) exeDir = exeDir.substr(0, pos);
		assetBase = exeDir + "/../assets";
	}

	PaintBackend resources(&backend);
	resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                    assetBase + "/rect_normal.ppm",
	                    assetBase + "/rect_hover.ppm",
	                    assetBase + "/rect_pressed.ppm", grayPalette);

	resources.addButton("Pencil", nullptr,
	                    assetBase + "/pencil_normal.ppm",
	                    assetBase + "/pencil_hover.ppm",
	                    assetBase + "/pencil_pressed.ppm", grayPalette);

	resources.addButton("Fill", nullptr,
	                    assetBase + "/fill_normal.ppm",
	                    assetBase + "/fill_hover.ppm",
	                    assetBase + "/fill_pressed.ppm", grayPalette);

	resources.addButton("Circle", nullptr,
	                    assetBase + "/circle_normal.ppm",
	                    assetBase + "/circle_hover.ppm",
	                    assetBase + "/circle_pressed.ppm", grayPalette);

	resources.addButton("Missing", nullptr,
	                    assetBase + "/no_normal.ppm",
	                    assetBase + "/no_hover.ppm",
	                    assetBase + "/no_pressed.ppm", grayPalette);

	resources.addButton("HoverOnly", nullptr,
	                    assetBase + "/no_normal2.ppm",
	                    assetBase + "/rect_hover.ppm",
	                    assetBase + "/no_pressed2.ppm", grayPalette);

	PaintFrontend main_window(resources);

	bool running = true;

	uint32_t FPS = 1000 / 60;    

	while (running)
	{
		// Procesar eventos SDL (y reenviarlos a ImGui)
		backend.poll_events(running);

		imgui_manager.begin_frame(backend);
		main_window.render();
		backend.render_imgui(imgui_manager.end_frame());

		auto start = SDL_GetTicks();
		while (1)
		{
			auto now = SDL_GetTicks();
			if (now - start > FPS)
				break;
		}

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
c) Funciones de composición
void blitWithClipping(ImageBuffer& dst, const OverlayRegion& src);
void resizeOverlay(OverlayRegion& overlay, const Rect& newBounds);


*/