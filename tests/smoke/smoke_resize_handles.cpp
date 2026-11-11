// Smoke test for the canvas resize handles and resize cursors.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_resize_handles.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_resize_handles

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

namespace
{
int g_failures = 0;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
		} \
	} while (0)

void push_motion(SDL_Window* window, int x, int y, Uint32 state)
{
	SDL_Event event{};
	event.type = SDL_MOUSEMOTION;
	event.motion.timestamp = SDL_GetTicks();
	event.motion.windowID = SDL_GetWindowID(window);
	event.motion.which = 0;
	event.motion.state = state;
	event.motion.x = x;
	event.motion.y = y;
	SDL_PushEvent(&event);
}

void push_button(SDL_Window* window, int x, int y, bool down)
{
	push_motion(window, x, y, down ? SDL_BUTTON_LMASK : 0);

	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.timestamp = SDL_GetTicks();
	event.button.windowID = SDL_GetWindowID(window);
	event.button.which = 0;
	event.button.button = SDL_BUTTON_LEFT;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = x;
	event.button.y = y;
	SDL_PushEvent(&event);
}

void save_frame(SDL_Renderer* renderer, const char* path)
{
	int width = 0;
	int height = 0;
	SDL_GetRendererOutputSize(renderer, &width, &height);
	if (width <= 0 || height <= 0)
	{
		return;
	}

	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
	if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24,
	                         pixels.data(), width * 3) != 0)
	{
		return;
	}

	FILE* file = std::fopen(path, "wb");
	if (!file)
	{
		return;
	}
	std::fprintf(file, "P6\n%d %d\n255\n", width, height);
	std::fwrite(pixels.data(), 1, pixels.size(), file);
	std::fclose(file);
}

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

int run()
{
	Harness h;
	h.imgui.init();
	h.backend.init();

	SDL_Window* window = nullptr;
	for (Uint32 id = 1; id <= 32 && !window; ++id)
	{
		SDL_Window* candidate = SDL_GetWindowFromID(id);
		if (candidate && std::strcmp(SDL_GetWindowTitle(candidate), "QtreGBPaint") == 0)
		{
			window = candidate;
		}
	}
	if (!window)
	{
		std::cerr << "SDL window not found\n";
		return 2;
	}
	SDL_Renderer* renderer = SDL_GetRenderer(window);
	if (!renderer)
	{
		std::cerr << "SDL renderer not found\n";
		return 2;
	}

	int corner_x = 0;
	int corner_y = 0;
	SDL_Cursor* arrow_cursor = nullptr;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			const PaintCore::Vec2f corner =
				h.frontend.m_canvas_area.m_view.image_to_screen(
					{256.0f, 256.0f});
			// Corner handle: 1 px gap + 4 px half of the 8x8 handle.
			corner_x = static_cast<int>(std::lround(corner.x)) + 5;
			corner_y = static_cast<int>(std::lround(corner.y)) + 5;
			CHECK(corner_x > 0);
			CHECK(corner_y > 0);

			arrow_cursor = SDL_GetCursor();
			CHECK(arrow_cursor != nullptr);
			push_button(window, corner_x, corner_y, true);
		}
		else if (frame == 3)
		{
			CHECK(SDL_GetCursor() != arrow_cursor);
			push_motion(window, corner_x + 50, corner_y + 50, SDL_BUTTON_LMASK);
		}
		else if (frame == 4)
		{
			save_frame(renderer, "temp/smoke_resize_handles.ppm");
			push_button(window, corner_x + 50, corner_y + 50, false);
		}
		else if (frame == 5)
		{
			PaintSession& canvas = h.frontend.m_canvas_area.session();
			CHECK(canvas.getWidth() == 306);
			CHECK(canvas.getHeight() == 306);
			CHECK(canvas.can_undo());
			CHECK(canvas.undo());
			CHECK(canvas.getWidth() == 256);
			CHECK(canvas.getHeight() == 256);
			CHECK(canvas.redo());
			CHECK(canvas.getWidth() == 306);
			CHECK(canvas.getHeight() == 306);
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		SDL_Delay(frame_ms);
		++frame;
	}

	h.backend.shutdown();
	h.imgui.shutdown();

	if (g_failures == 0)
	{
		std::cout << "SMOKE OK\n";
		return 0;
	}
	std::cerr << "SMOKE FAIL: " << g_failures << " failures\n";
	return 1;
}
}

int main()
{
	return run();
}
