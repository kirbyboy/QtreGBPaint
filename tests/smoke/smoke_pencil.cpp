// Smoke test: freehand pencil interpolation across fast mouse jumps.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ithird_party/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_pencil.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_pencil

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/line.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/pencil_tool.h"

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
	push_motion(window, x, y, down ? SDL_BUTTON(button) : 0);

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

void add_line(std::set<std::pair<int, int>>& points,
              int x0, int y0, int x1, int y1)
{
	PaintCore::rasterize_line(x0, y0, x1, y1,
		[&](int x, int y) { points.emplace(x, y); });
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

	PaintSession& canvas = h.frontend.m_backend.session();
	h.frontend.select_tool("Pencil");
	CHECK(h.resources.activeButton() == "Pencil");

	int canvas_x = 0;
	int canvas_y = 0;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		canvas_x = static_cast<int>(std::floor(
			h.frontend.m_canvas_area.m_view.origin.x));
		canvas_y = static_cast<int>(std::floor(
			h.frontend.m_canvas_area.m_view.origin.y));

		if (frame == 2)
		{
			push_motion(window, canvas_x + 2, canvas_y + 2, 0);
		}
		else if (frame == 3)
		{
			push_left(window, canvas_x + 2, canvas_y + 2, true);
		}
		else if (frame == 4)
		{
			push_motion(window, canvas_x + 20, canvas_y + 20,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 5)
		{
			push_motion(window, canvas_x + 40, canvas_y + 5,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 6)
		{
			push_left(window, canvas_x + 40, canvas_y + 5, false);
		}
		else if (frame == 9)
		{
			std::set<std::pair<int, int>> expected;
			add_line(expected, 2, 2, 20, 20);
			add_line(expected, 20, 20, 40, 5);

			int painted_count = 0;
			bool correct = true;
			for (int y = 0; y < 256; ++y)
			{
				for (int x = 0; x < 256; ++x)
				{
					const PaintCore::ColorIndex value =
						canvas.get_main_buffer().get_pixel(x, y);
					const bool should_be_painted =
						expected.count({x, y}) != 0;
					if (should_be_painted)
					{
						correct = correct && value == 3;
						++painted_count;
					}
					else if (value != 0)
					{
						correct = false;
					}
				}
			}

			CHECK(correct);
			CHECK_EQ(painted_count, static_cast<int>(expected.size()));
			CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 20), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(40, 5), 3);
			save_frame(renderer, "temp/smoke_pencil.ppm");
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
