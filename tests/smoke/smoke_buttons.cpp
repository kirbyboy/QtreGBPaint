// Smoke test: left/right/background drawing and both-button cancellation.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ithird_party/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_buttons.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_buttons

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
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
#include "paint/image_buffer.h"
#include "tools/pencil_tool.h"
#include "tools/square_tool.h"

namespace
{
int g_failures = 0;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
		} \
	} while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

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

void push_button(SDL_Window* window, int x, int y, Uint8 button, bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.timestamp = SDL_GetTicks();
	event.button.windowID = SDL_GetWindowID(window);
	event.button.which = 0;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = x;
	event.button.y = y;
	SDL_PushEvent(&event);
}

void warp(SDL_Window* window, int x, int y)
{
	SDL_WarpMouseInWindow(window, x, y);
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

	std::vector<std::uint8_t> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
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

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	Harness h;
	h.imgui.init();
	h.backend.init();

	std::string asset_base = "assets";
	if (char* base_path = SDL_GetBasePath())
	{
		asset_base = std::string(base_path) + "../assets";
		SDL_free(base_path);
	}

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Pencil", std::make_shared<PencilTool>(1, 2),
	                      PaintCore::CursorType::Pencil,
	                      asset_base + "/pencil_button.idx", gray);
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);

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

	PaintBackend& app = h.resources;
	PaintSession& canvas = app.session();
	app.set_foreground_index(3);
	app.set_background_index(1);

	int canvas_x = 0;
	int canvas_y = 0;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		canvas_x = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.x);
		canvas_y = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.y);

		if (frame == 2)
		{
			h.frontend.select_tool("Pencil");
			warp(window, canvas_x + 2, canvas_y + 2);
			push_motion(window, canvas_x + 2, canvas_y + 2, 0);
		}
		else if (frame == 3)
		{
			warp(window, canvas_x + 2, canvas_y + 2);
			push_motion(window, canvas_x + 2, canvas_y + 2, 0);
		}
		else if (frame == 4)
		{
			warp(window, canvas_x + 2, canvas_y + 2);
			push_button(window, canvas_x + 2, canvas_y + 2,
			            SDL_BUTTON_RIGHT, true);
		}
		else if (frame == 5)
		{
			warp(window, canvas_x + 20, canvas_y + 2);
			push_motion(window, canvas_x + 20, canvas_y + 2,
			            SDL_BUTTON_RMASK);
		}
		else if (frame == 6)
		{
			warp(window, canvas_x + 20, canvas_y + 2);
			push_button(window, canvas_x + 20, canvas_y + 2,
			            SDL_BUTTON_RIGHT, false);
		}
		else if (frame == 8)
		{
			for (int x = 2; x <= 20; ++x)
			{
				CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 2), 1);
			}
			CHECK(canvas.can_undo());
		}
		else if (frame == 9)
		{
			h.frontend.select_tool("Rect");
			if (ToolBase* tool = canvas.active_tool())
			{
				tool->set_option("mode", static_cast<int>(SquareTool::Mode::Both));
			}
			warp(window, canvas_x + 2, canvas_y + 40);
			push_motion(window, canvas_x + 2, canvas_y + 40, 0);
		}
		else if (frame == 10)
		{
			warp(window, canvas_x + 2, canvas_y + 40);
			push_button(window, canvas_x + 2, canvas_y + 40,
			            SDL_BUTTON_RIGHT, true);
		}
		else if (frame == 11)
		{
			warp(window, canvas_x + 11, canvas_y + 49);
			push_motion(window, canvas_x + 11, canvas_y + 49,
			            SDL_BUTTON_RMASK);
		}
		else if (frame == 12)
		{
			warp(window, canvas_x + 11, canvas_y + 49);
			push_button(window, canvas_x + 11, canvas_y + 49,
			            SDL_BUTTON_RIGHT, false);
		}
		else if (frame == 14)
		{
			// Right button swaps foreground/background for shapes.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 40), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(11, 49), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(6, 44), 3);
		}
		else if (frame == 15)
		{
			h.frontend.select_tool("Pencil");
			warp(window, canvas_x + 2, canvas_y + 80);
			push_motion(window, canvas_x + 2, canvas_y + 80, 0);
		}
		else if (frame == 16)
		{
			warp(window, canvas_x + 2, canvas_y + 80);
			push_button(window, canvas_x + 2, canvas_y + 80,
			            SDL_BUTTON_LEFT, true);
		}
		else if (frame == 17)
		{
			warp(window, canvas_x + 20, canvas_y + 80);
			push_motion(window, canvas_x + 20, canvas_y + 80,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 18)
		{
			// Second button cancels the unconfirmed left stroke.
			warp(window, canvas_x + 20, canvas_y + 80);
			push_button(window, canvas_x + 20, canvas_y + 80,
			            SDL_BUTTON_RIGHT, true);
		}
		else if (frame == 19)
		{
			warp(window, canvas_x + 20, canvas_y + 80);
			push_button(window, canvas_x + 20, canvas_y + 80,
			            SDL_BUTTON_RIGHT, false);
		}
		else if (frame == 20)
		{
			warp(window, canvas_x + 20, canvas_y + 80);
			push_button(window, canvas_x + 20, canvas_y + 80,
			            SDL_BUTTON_LEFT, false);
		}
		else if (frame == 23)
		{
			for (int x = 0; x < 30; ++x)
			{
				CHECK_EQ(canvas.get_main_buffer().get_pixel(x, 80), 0);
			}
			save_frame(renderer, "temp/smoke_buttons.ppm");
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
} // namespace

int main()
{
	return run();
}
