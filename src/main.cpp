#include <SDL.h>
#include "core/ibackend.h"
#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/paint_frontend.h"

int main()
{
	SDLRendererBackend backend;
	backend.init();

	ImGuiManager imguiManager;
	imguiManager.init();

	PaintFrontend main_window;

	bool running = true;
	SDL_Event event;

	uint32_t FPS = 1000 / 60;    

	while (running)
	{
		// Procesar eventos SDL
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
			else
			{
				backend.handle_event(&event);
			}
		}

		auto start = SDL_GetTicks();
		while (1)
		{
			auto now = SDL_GetTicks();
			if (now - start > FPS)
				break;
		}


		backend.new_frame();
		main_window.render();
		backend.render_draw_data();
	}

	backend.shutdown();

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