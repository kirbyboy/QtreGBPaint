// Smoke test: Fill tool through the ImGui frontend.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ilib/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_fill.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_fill

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
#include "core/rect.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/fill_tool.h"

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

void push_left(SDL_Window* window, int x, int y, bool down)
{
	push_button(window, x, y, SDL_BUTTON_LEFT, down);
}

void push_right(SDL_Window* window, int x, int y, bool down)
{
	push_button(window, x, y, SDL_BUTTON_RIGHT, down);
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

void set_rectangle(ImageBuffer& image, int x0, int y0, int x1, int y1,
                   std::uint8_t color)
{
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			image.set_pixel(x, y, color);
		}
	}
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
	h.resources.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                      PaintCore::CursorType::Fill,
	                      asset_base + "/fill_button.idx", gray);
	if (!h.resources.loadOptionIcon("fill.contiguous",
	                                asset_base + "/fill_options.idx",
	                                0, 0, 60, 46))
	{
		std::cerr << "contiguous option icon could not be loaded\n";
	}
	if (!h.resources.loadOptionIcon("fill.global",
	                                asset_base + "/fill_options.idx",
	                                0, 46, 60, 46))
	{
		std::cerr << "global option icon could not be loaded\n";
	}

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
	app.set_foreground_index(2);
	app.set_background_index(3);

	// Two disconnected regions of color 0 on a color-1 background:
	//   A: (3,3)..(7,7)
	//   B: (13,3)..(17,7)
	ImageBuffer image(32, 32, 2);
	image.fill(1);
	set_rectangle(image, 3, 3, 7, 7, 0);
	set_rectangle(image, 13, 3, 17, 7, 0);
	canvas.load_image(std::move(image));

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
			h.frontend.select_tool("Fill");
			warp(window, canvas_x + 5, canvas_y + 5);
			push_motion(window, canvas_x + 5, canvas_y + 5, 0);
		}
		else if (frame == 3)
		{
			warp(window, canvas_x + 5, canvas_y + 5);
			push_motion(window, canvas_x + 5, canvas_y + 5, 0);
		}
		else if (frame == 4)
		{
			warp(window, canvas_x + 5, canvas_y + 5);
			push_motion(window, canvas_x + 5, canvas_y + 5, 0);
			push_left(window, canvas_x + 5, canvas_y + 5, true);
		}
		else if (frame == 5)
		{
			push_left(window, canvas_x + 5, canvas_y + 5, false);
		}
		else if (frame == 7)
		{
			// Contiguous fill: only region A changes.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(15, 5), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
			save_frame(renderer, "temp/smoke_fill_contiguous.ppm");
		}
		else if (frame == 8)
		{
			CHECK(canvas.undo());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(15, 5), 0);
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK(tool->set_option(
					"mode", static_cast<int>(FillTool::Mode::Global)));
			}
		}
		else if (frame == 9)
		{
			warp(window, canvas_x + 15, canvas_y + 5);
			push_motion(window, canvas_x + 15, canvas_y + 5, 0);
		}
		else if (frame == 10)
		{
			warp(window, canvas_x + 15, canvas_y + 5);
			push_motion(window, canvas_x + 15, canvas_y + 5, 0);
			push_left(window, canvas_x + 15, canvas_y + 5, true);
		}
		else if (frame == 11)
		{
			push_left(window, canvas_x + 15, canvas_y + 5, false);
		}
		else if (frame == 13)
		{
			// Global fill: both disconnected regions change, background stays.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(15, 5), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 2), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
			save_frame(renderer, "temp/smoke_fill_global.ppm");
		}
		else if (frame == 14)
		{
			warp(window, canvas_x + 25, canvas_y + 25);
			push_motion(window, canvas_x + 25, canvas_y + 25, 0);
		}
		else if (frame == 15)
		{
			warp(window, canvas_x + 25, canvas_y + 25);
			push_motion(window, canvas_x + 25, canvas_y + 25, 0);
			push_right(window, canvas_x + 25, canvas_y + 25, true);
		}
		else if (frame == 16)
		{
			push_right(window, canvas_x + 25, canvas_y + 25, false);
		}
		else if (frame == 18)
		{
			// Right button fills with the background, leaving the interiors.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(25, 25), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(15, 5), 2);
			save_frame(renderer, "temp/smoke_fill_right.ppm");
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
