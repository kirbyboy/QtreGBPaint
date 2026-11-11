// Smoke test: Brush tool options, footprint preview and freehand stroke.
//
// Build from the repository root (or use tests/smoke/run.sh):
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ilib/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_brush.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_brush

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
#include "tools/brush_tool.h"

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

void push_left(SDL_Window* window, int x, int y, bool down)
{
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
	h.resources.addButton("Brush", std::make_shared<BrushTool>(1, 2),
	                      PaintCore::CursorType::Brush,
	                      asset_base + "/brush_button.idx", gray);
	std::string cursor_error;
	CHECK(h.resources.loadCursor(PaintCore::CursorType::Brush,
	                             asset_base + "/cursors/slim_cross.ipx",
	                             cursor_error));
	CHECK(cursor_error.empty());

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

	const auto option_cell_center = [](int column, int row,
	                                   float& x, float& y)
	{
		for (ImGuiWindow* window : GImGui->Windows)
		{
			if (!window || !window->Name ||
			    !std::strstr(window->Name, "ToolOptions"))
			{
				continue;
			}
			const float cell_width = window->InnerRect.GetWidth() / 3.0f;
			constexpr float kCellHeight = 24.0f;
			x = window->DC.CursorStartPos.x +
			    (column + 0.5f) * cell_width;
			y = window->DC.CursorStartPos.y +
			    (row + 0.5f) * kCellHeight;
			return true;
		}
		return false;
	};

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
			h.frontend.select_tool("Brush");
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
			CHECK(app.activeCursor() == PaintCore::CursorType::Brush);
			ToolBase* tool = canvas.active_tool();
			CHECK(tool != nullptr);
			if (tool)
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::CircleLarge));
			}
			save_frame(renderer, "temp/smoke_brush_hover.ppm");
		}
		else if (frame == 5)
		{
			warp(window, canvas_x + 10, canvas_y + 20);
			push_motion(window, canvas_x + 10, canvas_y + 20, 0);
			push_left(window, canvas_x + 10, canvas_y + 20, true);
		}
		else if (frame == 6)
		{
			warp(window, canvas_x + 30, canvas_y + 20);
			push_motion(window, canvas_x + 30, canvas_y + 20,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 7)
		{
			warp(window, canvas_x + 30, canvas_y + 20);
			push_left(window, canvas_x + 30, canvas_y + 20, false);
		}
		else if (frame == 9)
		{
			// Default circle-radius-3 brush: a seven-pixel-tall band with
			// rounded caps along the path.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(7, 20), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(33, 20), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 17), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 23), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 16), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 24), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(6, 20), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(34, 20), 0);
		}
		else if (frame == 10)
		{
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(1, 0, x, y))
			{
				warp(window, static_cast<int>(x), static_cast<int>(y));
				push_motion(window, static_cast<int>(x), static_cast<int>(y), 0);
				push_left(window, static_cast<int>(x), static_cast<int>(y), true);
			}
		}
		else if (frame == 11)
		{
			// Grid cells activate on press, so the option is already set
			// before the release below.
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::CircleMedium));
			}
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(1, 0, x, y))
			{
				push_left(window, static_cast<int>(x), static_cast<int>(y), false);
			}
		}
		else if (frame == 12)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::CircleMedium));
			}
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(2, 1, x, y))
			{
				warp(window, static_cast<int>(x), static_cast<int>(y));
				push_motion(window, static_cast<int>(x), static_cast<int>(y), 0);
				push_left(window, static_cast<int>(x), static_cast<int>(y), true);
			}
		}
		else if (frame == 13)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::SquareSmall));
			}
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(2, 1, x, y))
			{
				push_left(window, static_cast<int>(x), static_cast<int>(y), false);
			}
		}
		else if (frame == 14)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::SquareSmall));
			}
			warp(window, canvas_x + 10, canvas_y + 30);
			push_motion(window, canvas_x + 10, canvas_y + 30, 0);
		}
		else if (frame == 15)
		{
			warp(window, canvas_x + 10, canvas_y + 30);
			push_motion(window, canvas_x + 10, canvas_y + 30, 0);
			push_left(window, canvas_x + 10, canvas_y + 30, true);
		}
		else if (frame == 16)
		{
			warp(window, canvas_x + 20, canvas_y + 30);
			push_motion(window, canvas_x + 20, canvas_y + 30,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 17)
		{
			warp(window, canvas_x + 20, canvas_y + 30);
			push_left(window, canvas_x + 20, canvas_y + 30, false);
		}
		else if (frame == 19)
		{
			// Square-2 brush: a two-pixel-tall band, with the hotspot at its
			// top-left corner.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 30), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(21, 31), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(9, 30), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 32), 0);

			// Select the length-5 down-right diagonal from the grid.
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(1, 2, x, y))
			{
				warp(window, static_cast<int>(x), static_cast<int>(y));
				push_motion(window, static_cast<int>(x), static_cast<int>(y), 0);
				push_left(window, static_cast<int>(x), static_cast<int>(y), true);
			}
		}
		else if (frame == 20)
		{
			float x = 0.0f;
			float y = 0.0f;
			if (option_cell_center(1, 2, x, y))
			{
				push_left(window, static_cast<int>(x), static_cast<int>(y), false);
			}
		}
		else if (frame == 21)
		{
			if (ToolBase* tool = canvas.active_tool())
			{
				CHECK_EQ(tool->option_value("brush"),
				         static_cast<int>(BrushTool::Brush::DiagonalDownMedium));
			}
			warp(window, canvas_x + 10, canvas_y + 40);
			push_motion(window, canvas_x + 10, canvas_y + 40, 0);
		}
		else if (frame == 22)
		{
			warp(window, canvas_x + 10, canvas_y + 40);
			push_motion(window, canvas_x + 10, canvas_y + 40, 0);
			push_left(window, canvas_x + 10, canvas_y + 40, true);
		}
		else if (frame == 23)
		{
			warp(window, canvas_x + 16, canvas_y + 34);
			push_motion(window, canvas_x + 16, canvas_y + 34,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 24)
		{
			warp(window, canvas_x + 16, canvas_y + 34);
			push_left(window, canvas_x + 16, canvas_y + 34, false);
		}
		else if (frame == 26)
		{
			// Perfect anti-diagonal movement with a down-right brush: the
			// corner fill keeps the stroke continuous at (11,40).
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 40), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(11, 40), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(16, 34), 3);
			save_frame(renderer, "temp/smoke_brush.ppm");
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
