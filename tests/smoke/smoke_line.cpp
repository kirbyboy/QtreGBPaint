// Smoke test: straight-line tool drawing through the real frontend.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ilib/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_line.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_line

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
#include "core/line_stamp.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
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

bool option_row_center(int row, float& x, float& y)
{
	for (ImGuiWindow* window : GImGui->Windows)
	{
		if (!window || !window->Name ||
		    !std::strstr(window->Name, "ToolOptions"))
		{
			continue;
		}
		const float row_height = window->InnerRect.GetHeight() / 5.0f;
		x = window->InnerRect.Min.x + window->InnerRect.GetWidth() * 0.5f;
		y = window->InnerRect.Min.y + (row + 0.5f) * row_height;
		return true;
	}
	return false;
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
	h.frontend.select_tool("Line");
	CHECK(h.resources.activeButton() == "Line");

	int canvas_x = 0;
	int canvas_y = 0;
	float option_x = 0.0f;
	float option_y = 0.0f;
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
			// A single jump simulates a fast mouse move. The line tool must
			// draw the complete segment, not only the endpoints.
			push_motion(window, canvas_x + 30, canvas_y + 20,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 5)
		{
			push_left(window, canvas_x + 30, canvas_y + 20, false);
		}
		else if (frame == 8)
		{
			std::vector<bool> expected(256 * 256, false);
			int expected_count = 0;
			PaintCore::rasterize_line(2, 2, 30, 20,
				[&](int x, int y)
				{
					expected[static_cast<std::size_t>(y) * 256 +
					         static_cast<std::size_t>(x)] = true;
					++expected_count;
				});

			int painted_count = 0;
			bool correct = true;
			for (int y = 0; y < 256; ++y)
			{
				for (int x = 0; x < 256; ++x)
				{
					const PaintCore::ColorIndex value =
						canvas.get_main_buffer().get_pixel(x, y);
					const bool should_be_painted =
						expected[static_cast<std::size_t>(y) * 256 +
						         static_cast<std::size_t>(x)];
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
			CHECK_EQ(painted_count, expected_count);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 2), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(30, 20), 3);
			save_frame(renderer, "temp/smoke_line.ppm");
		}
		else if (frame == 9)
		{
			// Fifth width row: five-pixel lines.
			CHECK(option_row_center(4, option_x, option_y));
			warp(window, static_cast<int>(option_x),
			     static_cast<int>(option_y));
			push_motion(window, static_cast<int>(option_x),
			            static_cast<int>(option_y), 0);
		}
		else if (frame == 10)
		{
			push_left(window, static_cast<int>(option_x),
			          static_cast<int>(option_y), true);
		}
		else if (frame == 11)
		{
			// Option changes must happen on press, before the release.
			LineTool* tool = dynamic_cast<LineTool*>(canvas.active_tool());
			CHECK(tool != nullptr);
			if (tool)
			{
				CHECK_EQ(tool->option_value("width"), 5);
			}
			push_left(window, static_cast<int>(option_x),
			          static_cast<int>(option_y), false);
		}
		else if (frame == 12)
		{
			warp(window, canvas_x + 2, canvas_y + 60);
			push_motion(window, canvas_x + 2, canvas_y + 60, 0);
			push_left(window, canvas_x + 2, canvas_y + 60, true);
		}
		else if (frame == 13)
		{
			warp(window, canvas_x + 30, canvas_y + 60);
			push_motion(window, canvas_x + 30, canvas_y + 60,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 14)
		{
			push_left(window, canvas_x + 30, canvas_y + 60, false);
		}
		else if (frame == 16)
		{
			std::set<std::pair<int, int>> expected;
			const auto add_stroke = [&](int x0, int y0, int x1, int y1,
			                            int width)
			{
				PaintCore::rasterize_line(
					x0, y0, x1, y1,
					[&](int x, int y)
					{
						for (const PaintCore::Vec2& offset :
						     PaintCore::make_line_stamp_offsets(width))
						{
							expected.insert({x + offset.x, y + offset.y});
						}
					});
			};
			add_stroke(2, 2, 30, 20, 1);
			add_stroke(2, 60, 30, 60, 5);

			std::set<std::pair<int, int>> painted;
			for (int y = 0; y < 256; ++y)
			{
				for (int x = 0; x < 256; ++x)
				{
					if (canvas.get_main_buffer().get_pixel(x, y) == 3)
					{
						painted.insert({x, y});
					}
				}
			}

			CHECK_EQ(painted, expected);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 58), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 60), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 62), 3);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 57), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 63), 0);
			save_frame(renderer, "temp/smoke_line_width.ppm");
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
