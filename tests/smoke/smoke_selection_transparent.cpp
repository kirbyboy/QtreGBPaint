// Smoke test for Selection's transparent-background mode through the real
// toolbar option and canvas interaction.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_selection_transparent.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_selection_transparent

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "paint/image_buffer.h"
#include "tools/base_tool.h"
#include "tools/selection_tool.h"
#include "tools/square_tool.h"

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

#define CHECK_FALSE(expr) CHECK(!(expr))
#define CHECK_EQ(a, b) CHECK((a) == (b))

int canvas_x = 0;
int canvas_y = 0;

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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};

	PaintSession& canvas() { return frontend.m_canvas_area.session(); }
};

bool rect_eq(const PaintCore::Rect& a, const PaintCore::Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	Harness h;
	h.imgui.init();
	h.backend.init();

	std::string asset_base = "assets";
	if (char* basePath = SDL_GetBasePath())
	{
		asset_base = std::string(basePath) + "../assets";
		SDL_free(basePath);
	}

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/selection_button.idx", gray);
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);

	CHECK(h.resources.loadOptionIcon("selection.normal",
	                                 asset_base + "/selection_options.idx",
	                                 0, 0, 60, 16));
	CHECK(h.resources.loadOptionIcon("selection.transparent",
	                                 asset_base + "/selection_options.idx",
	                                 0, 16, 60, 16));

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

	PaintSession& canvas = h.canvas();

	// Hollow 6x6 square: border index 3, interior background index 0.
	std::vector<PaintCore::ColorIndex> source(
		static_cast<std::size_t>(6) * 6);
	for (int y = 10; y <= 15; ++y)
	{
		for (int x = 10; x <= 15; ++x)
		{
			const bool border =
				x == 10 || y == 10 || x == 15 || y == 15;
			source[static_cast<std::size_t>(y - 10) * 6 +
			       static_cast<std::size_t>(x - 10)] =
				border ? 3 : 0;
		}
	}
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{10, 10, 6, 6}, source);

	// Destination area, so transparent content reveals something different.
	const std::vector<PaintCore::ColorIndex> destination(
		static_cast<std::size_t>(6) * 6, 2);
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{20, 20, 6, 6}, destination);
	canvas.clear_history();

	// The smoke checks pixel indices assuming the classic dark fg / light bg.
	h.resources.set_foreground_index(3);
	h.resources.set_background_index(0);

	h.frontend.select_tool("Selection");

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
			// Second option row: Transparent background.
			push_motion(window, 54, 170, 0);
		}
		else if (frame == 3)
		{
			push_left(window, 54, 170, true);
		}
		else if (frame == 4)
		{
			push_left(window, 54, 170, false);
		}
		else if (frame == 5)
		{
			SelectionTool* tool =
				dynamic_cast<SelectionTool*>(canvas.active_tool());
			CHECK(tool != nullptr);
			if (tool)
			{
				CHECK_EQ(tool->option_value("mode"),
				         static_cast<int>(SelectionTool::Mode::Transparent));
			}
		}
		else if (frame == 6)
		{
			push_motion(window, canvas_x + 10, canvas_y + 10, 0);
		}
		else if (frame == 7)
		{
			push_left(window, canvas_x + 10, canvas_y + 10, true);
		}
		else if (frame == 8)
		{
			push_motion(window, canvas_x + 15, canvas_y + 15,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 9)
		{
			push_left(window, canvas_x + 15, canvas_y + 15, false);
		}
		else if (frame == 10)
		{
			CHECK(canvas.has_floating_region());
			CHECK(canvas.floating_region().transparent_background);
			CHECK_EQ(canvas.floating_region().transparent_color, 0);
			CHECK(rect_eq(canvas.floating_region().rect,
			              PaintCore::Rect{10, 10, 6, 6}));
			CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 3);
			// Raw content: transparency is a live mask, not baked pixels.
			CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 0);
			save_frame(renderer, "temp/smoke_selection_transparent.ppm");
		}
		else if (frame == 11)
		{
			// First option row: Normal.
			push_motion(window, 54, 120, 0);
		}
		else if (frame == 12)
		{
			push_left(window, 54, 120, true);
		}
		else if (frame == 13)
		{
			push_left(window, 54, 120, false);
		}
		else if (frame == 14)
		{
			CHECK_FALSE(canvas.floating_region().transparent_background);
			CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 0);
			save_frame(renderer, "temp/smoke_selection_transparent_opaque.ppm");
		}
		else if (frame == 15)
		{
			// Second option row: Transparent again.
			push_motion(window, 54, 170, 0);
		}
		else if (frame == 16)
		{
			push_left(window, 54, 170, true);
		}
		else if (frame == 17)
		{
			push_left(window, 54, 170, false);
		}
		else if (frame == 18)
		{
			CHECK(canvas.floating_region().transparent_background);
			CHECK_EQ(canvas.floating_region().content().get_pixel(1, 1), 0);
		}
		else if (frame == 19)
		{
			CHECK(canvas.move_floating_region(10, 10));
		}
		else if (frame == 20)
		{
			CHECK(rect_eq(canvas.floating_region().rect,
			              PaintCore::Rect{20, 20, 6, 6}));
			save_frame(renderer, "temp/smoke_selection_transparent_moved.ppm");
		}
		else if (frame == 21)
		{
			push_motion(window, canvas_x + 100, canvas_y + 100, 0);
		}
		else if (frame == 22)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, true);
		}
		else if (frame == 23)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, false);
		}
		else if (frame == 24)
		{
			CHECK_FALSE(canvas.has_floating_region());
			// Hole filled with background.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 10), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(15, 15), 0);
			// Destination: border written, transparent interior preserved.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 20), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(25, 25), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(21, 21), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(24, 24), 2);
			CHECK(canvas.can_undo());
			save_frame(renderer, "temp/smoke_selection_transparent_committed.ppm");
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
