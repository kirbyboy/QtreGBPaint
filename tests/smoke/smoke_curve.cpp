// Smoke test: Curve tool through the real frontend. Draws the initial line,
// drags a control handle, commits with a click outside the image and checks
// the rasterized result.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ilib/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_curve.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_curve

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
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/bezier.h"
#include "core/imgui_manager.h"
#include "core/rect.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/curve_tool.h"
#include "tools/line_tool.h"

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
#define REQUIRE(expr) CHECK(expr)
#define REQUIRE_EQ(a, b) CHECK_EQ(a, b)

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

void push_key(SDL_Window* window, SDL_Keycode key, SDL_Scancode scancode,
              bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.timestamp = SDL_GetTicks();
	event.key.windowID = SDL_GetWindowID(window);
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = 0;
	event.key.keysym.sym = key;
	event.key.keysym.scancode = scancode;
	SDL_PushEvent(&event);
}

void push_escape(SDL_Window* window, bool down)
{
	push_key(window, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, down);
}

void push_enter(SDL_Window* window, bool down)
{
	push_key(window, SDLK_RETURN, SDL_SCANCODE_RETURN, down);
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
	h.resources.addButton("Line", std::make_shared<LineTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/line_button.idx", gray);
	h.resources.addButton("Curve", std::make_shared<CurveTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/curve_button.idx", gray);

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
	app.set_background_index(1);

	ImageBuffer image(32, 32, 2);
	image.fill(0);
	canvas.load_image(std::move(image));
	h.frontend.m_zoom = 8;

	int canvas_x = 0;
	int canvas_y = 0;
	constexpr int kZoom = 8;
	const auto sx = [&](int x) { return canvas_x + x * kZoom; };
	const auto sy = [&](int y) { return canvas_y + y * kZoom; };
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		canvas_x = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.x);
		canvas_y = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.y);

		if (frame == 2)
		{
			h.frontend.select_tool("Curve");
			warp(window, sx(4), sy(16));
			push_motion(window, sx(4), sy(16), 0);
		}
		else if (frame == 3)
		{
			warp(window, sx(4), sy(16));
			push_motion(window, sx(4), sy(16), 0);
		}
		else if (frame == 4)
		{
			warp(window, sx(4), sy(16));
			push_motion(window, sx(4), sy(16), 0);
			push_left(window, sx(4), sy(16), true);
		}
		else if (frame == 5)
		{
			warp(window, sx(28), sy(16));
			push_motion(window, sx(28), sy(16),
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 6)
		{
			push_left(window, sx(28), sy(16), false);
		}
		else if (frame == 8)
		{
			CHECK(canvas.has_pending_edit());
			ToolBase* tool = canvas.active_tool();
			REQUIRE(tool != nullptr);
			ToolBase::EditOverlay overlay;
			REQUIRE(tool->edit_overlay(overlay));
			REQUIRE_EQ(overlay.handles.size(), 4u);
			CHECK_EQ(overlay.handles[1].pos.x, 12);
			CHECK_EQ(overlay.handles[1].pos.y, 16);
			save_frame(renderer, "temp/smoke_curve_pending.ppm");
		}
		else if (frame == 9)
		{
			// Drag P1 from (12,16) to (10,4).
			warp(window, sx(12), sy(16));
			push_motion(window, sx(12), sy(16), 0);
		}
		else if (frame == 10)
		{
			warp(window, sx(12), sy(16));
			push_motion(window, sx(12), sy(16), 0);
			push_left(window, sx(12), sy(16), true);
		}
		else if (frame == 11)
		{
			warp(window, sx(10), sy(4));
			push_motion(window, sx(10), sy(4),
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 12)
		{
			push_left(window, sx(10), sy(4), false);
		}
		else if (frame == 14)
		{
			CHECK(canvas.has_pending_edit());
			ToolBase* tool = canvas.active_tool();
			REQUIRE(tool != nullptr);
			ToolBase::EditOverlay overlay;
			REQUIRE(tool->edit_overlay(overlay));
			CHECK_EQ(overlay.handles[1].pos.x, 10);
			CHECK_EQ(overlay.handles[1].pos.y, 4);
			save_frame(renderer, "temp/smoke_curve_handles.ppm");

			// Click in the canvas margin (outside the image) to commit.
			warp(window, sx(32) + 4, sy(32) + 4);
			push_motion(window, sx(32) + 4, sy(32) + 4, 0);
			push_left(window, sx(32) + 4, sy(32) + 4, true);
		}
		else if (frame == 15)
		{
			push_left(window, sx(32) + 4, sy(32) + 4, false);
		}
		else if (frame == 17)
		{
			CHECK_FALSE(canvas.has_pending_edit());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 16), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 16), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(28, 16), 2);
			save_frame(renderer, "temp/smoke_curve_committed.ppm");

			// Start another curve and commit it by switching tools.
			warp(window, sx(4), sy(24));
			push_motion(window, sx(4), sy(24), 0);
			push_left(window, sx(4), sy(24), true);
		}
		else if (frame == 18)
		{
			warp(window, sx(28), sy(24));
			push_motion(window, sx(28), sy(24),
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 19)
		{
			push_left(window, sx(28), sy(24), false);
		}
		else if (frame == 21)
		{
			CHECK(canvas.has_pending_edit());
			h.frontend.select_tool("Line");
			CHECK_FALSE(canvas.has_pending_edit());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 24), 2);
			save_frame(renderer, "temp/smoke_curve_tool_switch.ppm");

			// A new curve cancelled with Escape must not touch the image.
			h.frontend.select_tool("Curve");
			warp(window, sx(4), sy(4));
			push_motion(window, sx(4), sy(4), 0);
		}
		else if (frame == 22)
		{
			warp(window, sx(4), sy(4));
			push_motion(window, sx(4), sy(4), 0);
			push_left(window, sx(4), sy(4), true);
		}
		else if (frame == 23)
		{
			warp(window, sx(28), sy(4));
			push_motion(window, sx(28), sy(4), SDL_BUTTON_LMASK);
		}
		else if (frame == 24)
		{
			push_left(window, sx(28), sy(4), false);
		}
		else if (frame == 26)
		{
			CHECK(canvas.has_pending_edit());
			push_escape(window, true);
		}
		else if (frame == 27)
		{
			push_escape(window, false);
		}
		else if (frame == 29)
		{
			CHECK_FALSE(canvas.has_pending_edit());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 4), 0);

			// And a new curve committed with Enter must be rasterized.
			warp(window, sx(4), sy(4));
			push_motion(window, sx(4), sy(4), 0);
			push_left(window, sx(4), sy(4), true);
		}
		else if (frame == 30)
		{
			warp(window, sx(28), sy(4));
			push_motion(window, sx(28), sy(4), SDL_BUTTON_LMASK);
		}
		else if (frame == 31)
		{
			push_left(window, sx(28), sy(4), false);
		}
		else if (frame == 33)
		{
			CHECK(canvas.has_pending_edit());
			push_enter(window, true);
		}
		else if (frame == 34)
		{
			push_enter(window, false);
		}
		else if (frame == 36)
		{
			CHECK_FALSE(canvas.has_pending_edit());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(28, 4), 2);
			save_frame(renderer, "temp/smoke_curve_enter.ppm");

			// A five-pixel curve must sweep the cross footprint before and
			// after commit, not only under the pointer path.
			CurveTool* tool = dynamic_cast<CurveTool*>(canvas.active_tool());
			REQUIRE(tool != nullptr);
			if (tool)
			{
				REQUIRE(tool->set_option("width", 5));
			}
			warp(window, sx(4), sy(28));
			push_motion(window, sx(4), sy(28), 0);
			push_left(window, sx(4), sy(28), true);
		}
		else if (frame == 37)
		{
			warp(window, sx(28), sy(28));
			push_motion(window, sx(28), sy(28), SDL_BUTTON_LMASK);
		}
		else if (frame == 38)
		{
			push_left(window, sx(28), sy(28), false);
		}
		else if (frame == 40)
		{
			CHECK(canvas.has_pending_edit());
			push_enter(window, true);
		}
		else if (frame == 41)
		{
			push_enter(window, false);
		}
		else if (frame == 43)
		{
			CHECK_FALSE(canvas.has_pending_edit());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 26), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 27), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 28), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 29), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 30), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 25), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 31), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 28), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 28), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(30, 28), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(31, 28), 0);
			save_frame(renderer, "temp/smoke_curve_width.ppm");
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
