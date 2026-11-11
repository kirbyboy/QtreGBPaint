// Smoke test: square eraser preview, left/right erase modes and size grid.
//
// Build from the repository root (or use tests/smoke/run.sh):
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ilib/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_eraser.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_eraser

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
#include "paint/draw_buffer.h"
#include "paint/image_buffer.h"
#include "tools/eraser_tool.h"

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

void fill_canvas(PaintSession& canvas, PaintCore::ColorIndex color)
{
	DrawBuffer buffer;
	buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	buffer.begin(0, 0);
	buffer.ensure_fit(canvas.getWidth() - 1, canvas.getHeight() - 1);
	for (int y = 0; y < canvas.getHeight(); ++y)
	{
		for (int x = 0; x < canvas.getWidth(); ++x)
		{
			buffer.set_pixel(x, y, color);
		}
	}
	canvas.commit_draw_buffer(buffer);
	canvas.clear_history();
}

void set_pixel(PaintSession& canvas, int x, int y, PaintCore::ColorIndex color)
{
	DrawBuffer buffer;
	buffer.set_bounds(canvas.getWidth(), canvas.getHeight());
	buffer.begin(0, 0);
	buffer.ensure_fit(x, y);
	buffer.set_pixel(x, y, color);
	canvas.commit_draw_buffer(buffer);
	canvas.clear_history();
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
	h.resources.addButton("Eraser", std::make_shared<EraserTool>(3, 1),
	                      PaintCore::CursorType::Eraser,
	                      asset_base + "/eraser_button.idx", gray);
	CHECK(h.resources.setCursorImage(PaintCore::CursorType::Eraser,
	                                 PaintCore::make_invisible_cursor()));

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
	fill_canvas(canvas, 3);

	const auto option_row_center = [](int row, float& x, float& y)
	{
		for (ImGuiWindow* window : GImGui->Windows)
		{
			if (!window || !window->Name ||
			    !std::strstr(window->Name, "ToolOptions"))
			{
				continue;
			}
			const float width = window->InnerRect.GetWidth();
			constexpr float kRowHeight = 24.0f;
			x = window->DC.CursorStartPos.x + width * 0.5f;
			y = window->DC.CursorStartPos.y +
			    (row + 0.5f) * kRowHeight;
			return true;
		}
		return false;
	};

	SDL_Cursor* arrow_cursor = nullptr;

	int canvas_x = 0;
	int canvas_y = 0;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		canvas_x = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.x);
		canvas_y = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.y);

		if (frame == 1)
		{
			arrow_cursor = SDL_GetCursor();
			CHECK(arrow_cursor != nullptr);
		}
		else if (frame == 2)
		{
			h.frontend.select_tool("Eraser");
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
			CHECK(app.activeCursor() == PaintCore::CursorType::Eraser);
			SDL_Cursor* eraser_cursor = SDL_GetCursor();
			CHECK(eraser_cursor != nullptr);
			CHECK(eraser_cursor != arrow_cursor);
			ToolBase* tool = canvas.active_tool();
			CHECK(tool != nullptr);
			if (tool)
			{
				CHECK_EQ(tool->option_value("size"),
				         static_cast<int>(EraserTool::Size::Tiny));
			}
			save_frame(renderer, "temp/smoke_eraser_hover.ppm");

			// Select the largest eraser (last row) before drawing the band.
			float x = 0.0f;
			float y = 0.0f;
			if (option_row_center(3, x, y))
			{
				warp(window, static_cast<int>(x), static_cast<int>(y));
				push_motion(window, static_cast<int>(x), static_cast<int>(y), 0);
				push_left(window, static_cast<int>(x), static_cast<int>(y), true);
			}
		}
		else if (frame == 5)
		{
			float x = 0.0f;
			float y = 0.0f;
			if (option_row_center(3, x, y))
			{
				push_left(window, static_cast<int>(x), static_cast<int>(y), false);
			}
		}
		else if (frame == 6)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("size"),
				         static_cast<int>(EraserTool::Size::Large));
			}
			warp(window, canvas_x + 10, canvas_y + 20);
			push_motion(window, canvas_x + 10, canvas_y + 20, 0);
			push_left(window, canvas_x + 10, canvas_y + 20, true);
		}
		else if (frame == 7)
		{
			warp(window, canvas_x + 30, canvas_y + 20);
			push_motion(window, canvas_x + 30, canvas_y + 20,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 8)
		{
			warp(window, canvas_x + 30, canvas_y + 20);
			push_left(window, canvas_x + 30, canvas_y + 20, false);
		}
		else if (frame == 10)
		{
			// Size 10 around y=20: a background band from x=6..35,
			// y=16..25.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(6, 16), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(35, 25), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 20), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 20), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(36, 20), 3);

			// Select the smallest eraser (first row) for the right click.
			float x = 0.0f;
			float y = 0.0f;
			if (option_row_center(0, x, y))
			{
				warp(window, static_cast<int>(x), static_cast<int>(y));
				push_motion(window, static_cast<int>(x), static_cast<int>(y), 0);
				push_left(window, static_cast<int>(x), static_cast<int>(y), true);
			}
		}
		else if (frame == 11)
		{
			float x = 0.0f;
			float y = 0.0f;
			if (option_row_center(0, x, y))
			{
				push_left(window, static_cast<int>(x), static_cast<int>(y), false);
			}
		}
		else if (frame == 12)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("size"),
				         static_cast<int>(EraserTool::Size::Tiny));
			}

			set_pixel(canvas, 30, 26, 2);
			warp(window, canvas_x + 30, canvas_y + 25);
			push_motion(window, canvas_x + 30, canvas_y + 25, 0);
		}
		else if (frame == 13)
		{
			warp(window, canvas_x + 30, canvas_y + 25);
			push_motion(window, canvas_x + 30, canvas_y + 25, 0);
			push_right(window, canvas_x + 30, canvas_y + 25, true);
		}
		else if (frame == 14)
		{
			warp(window, canvas_x + 30, canvas_y + 25);
			push_right(window, canvas_x + 30, canvas_y + 25, false);
		}
		else if (frame == 16)
		{
			// Size 4 at (30,25) covers x=29..32, y=24..27. Only foreground
			// pixels become background; (30,26)=2 must stay untouched.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(30, 26), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(31, 26), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(32, 27), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(30, 24), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(40, 40), 3);
			save_frame(renderer, "temp/smoke_eraser.ppm");
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
