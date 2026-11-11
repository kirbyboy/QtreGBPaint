// Smoke test: Pick Color through the real frontend. Left/right clicks sample
// the foreground/background and return to the previous editing tool.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ithird_party/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_pick_color.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_pick_color

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
#include "tools/pick_color_tool.h"
#include "tools/selection_tool.h"

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
#define CHECK_FALSE(expr) CHECK(!(expr))

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
	h.resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/selection_button.idx", gray);
	h.resources.addButton("Pick Color", std::make_shared<PickColorTool>(),
	                      PaintCore::CursorType::Pick,
	                      asset_base + "/pick_button.idx", gray);

	std::string cursor_error;
	CHECK(h.resources.loadCursor("Pick Color",
	                             asset_base + "/cursors/pick.ipx",
	                             cursor_error));

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
	app.set_foreground_index(0);
	app.set_background_index(1);

	ImageBuffer image(32, 32, 2);
	image.fill(0);
	image.set_pixel(10, 10, 2);
	image.set_pixel(20, 10, 3);
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
			h.frontend.select_tool("Pick Color");
			CHECK(app.activeCursor() == PaintCore::CursorType::Pick);
			warp(window, canvas_x + 10, canvas_y + 10);
			push_motion(window, canvas_x + 10, canvas_y + 10, 0);
		}
		else if (frame == 3)
		{
			warp(window, canvas_x + 10, canvas_y + 10);
			push_motion(window, canvas_x + 10, canvas_y + 10, 0);
		}
		else if (frame == 4)
		{
			warp(window, canvas_x + 10, canvas_y + 10);
			push_motion(window, canvas_x + 10, canvas_y + 10, 0);
			push_left(window, canvas_x + 10, canvas_y + 10, true);
		}
		else if (frame == 5)
		{
			push_left(window, canvas_x + 10, canvas_y + 10, false);
		}
		else if (frame == 7)
		{
			CHECK_EQ(app.foreground_index(), 2);
			CHECK(app.activeButton() == "Fill");
			CHECK_FALSE(canvas.can_undo());
			save_frame(renderer, "temp/smoke_pick_color_left.ppm");

			// Selecting Selection must not replace the remembered Fill tool.
			h.frontend.select_tool("Selection");
			h.frontend.select_tool("Pick Color");
			CHECK(app.activeCursor() == PaintCore::CursorType::Pick);
			warp(window, canvas_x + 20, canvas_y + 10);
			push_motion(window, canvas_x + 20, canvas_y + 10, 0);
		}
		else if (frame == 8)
		{
			warp(window, canvas_x + 20, canvas_y + 10);
			push_motion(window, canvas_x + 20, canvas_y + 10, 0);
		}
		else if (frame == 9)
		{
			warp(window, canvas_x + 20, canvas_y + 10);
			push_motion(window, canvas_x + 20, canvas_y + 10, 0);
			push_right(window, canvas_x + 20, canvas_y + 10, true);
		}
		else if (frame == 10)
		{
			push_right(window, canvas_x + 20, canvas_y + 10, false);
		}
		else if (frame == 12)
		{
			CHECK_EQ(app.background_index(), 3);
			CHECK_EQ(app.foreground_index(), 2);
			CHECK(app.activeButton() == "Fill");
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 10), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 10), 3);
			save_frame(renderer, "temp/smoke_pick_color_right.ppm");
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
