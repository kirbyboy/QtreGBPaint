// Smoke test: status bar, two-point drag coordinates and the eyedropper
// preview/behavior through the real frontend.
//
// Captures:
//   temp/smoke_status_idle.ppm   status bar with coordinates and hover color
//   temp/smoke_status_drag.ppm   two coordinates while dragging a rectangle
//   temp/smoke_status_pick.ppm   options area filled with the eyedropper color

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

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/pick_color_tool.h"
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
#define CHECK_FALSE(expr) CHECK(!(expr))
#define REQUIRE(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
			return 1; \
		} \
	} while (0)

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

bool status_window_exists()
{
	for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
	{
		if (window->Name && std::strstr(window->Name, "/Status_") != nullptr)
		{
			return true;
		}
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
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);
	h.resources.addButton("Pick Color", std::make_shared<PickColorTool>(),
	                      PaintCore::CursorType::Pick,
	                      asset_base + "/pick_button.idx", gray);

	SDL_Window* window = nullptr;
	for (Uint32 id = 1; id <= 32 && !window; ++id)
	{
		SDL_Window* candidate = SDL_GetWindowFromID(id);
		if (candidate &&
		    std::strcmp(SDL_GetWindowTitle(candidate), "QtreGBPaint") == 0)
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
	app.set_background_index(0);

	ImageBuffer image(32, 32, 2);
	image.fill(0);
	image.set_pixel(20, 10, 3);
	image.set_pixel(20, 20, 2);
	canvas.load_image(std::move(image));

	int canvas_x = 0;
	int canvas_y = 0;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;
	const auto pixel_pos = [&](int x, int y)
	{
		return std::pair<int, int>(
			canvas_x + x * 8 + 4, canvas_y + y * 8 + 4);
	};

	while (running)
	{
		canvas_x = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.x);
		canvas_y = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.y);

		if (frame == 2)
		{
			h.frontend.m_zoom = 8;
			h.frontend.select_tool("Rect");
		}
		else if (frame == 3)
		{
			CHECK(status_window_exists());
			const auto p = pixel_pos(5, 5);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
		}
		else if (frame == 4)
		{
			save_frame(renderer, "temp/smoke_status_idle.ppm");

			const auto p = pixel_pos(5, 5);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
			push_left(window, p.first, p.second, true);
		}
		else if (frame == 5)
		{
			const auto p = pixel_pos(10, 10);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, SDL_BUTTON_LMASK);
		}
		else if (frame == 6)
		{
			save_frame(renderer, "temp/smoke_status_drag.ppm");

			ToolBase* tool = canvas.active_tool();
			REQUIRE(tool != nullptr);
			PaintCore::Vec2 anchor{};
			REQUIRE(tool->drag_anchor(anchor));
			CHECK_EQ(anchor.x, 5);
			CHECK_EQ(anchor.y, 5);

			const auto p = pixel_pos(10, 10);
			push_left(window, p.first, p.second, false);
		}
		else if (frame == 8)
		{
			h.frontend.select_tool("Pick Color");
			const auto p = pixel_pos(20, 10);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
		}
		else if (frame == 9)
		{
			const auto p = pixel_pos(20, 10);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
			push_left(window, p.first, p.second, true);
		}
		else if (frame == 10)
		{
			const auto p = pixel_pos(20, 20);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, SDL_BUTTON_LMASK);

			ToolBase* tool = canvas.active_tool();
			REQUIRE(tool != nullptr);
			PaintCore::ColorIndex preview = 0;
			REQUIRE(tool->color_pick_preview(preview));
			CHECK_EQ(preview, 3);
		}
		else if (frame == 11)
		{
			save_frame(renderer, "temp/smoke_status_pick.ppm");

			ToolBase* tool = canvas.active_tool();
			REQUIRE(tool != nullptr);
			PaintCore::ColorIndex preview = 0;
			REQUIRE(tool->color_pick_preview(preview));
			CHECK_EQ(preview, 2);
		}
		else if (frame == 12)
		{
			const auto p = pixel_pos(20, 20);
			push_left(window, p.first, p.second, false);
		}
		else if (frame == 14)
		{
			CHECK_EQ(app.foreground_index(), 2);
			CHECK(app.activeButton() == "Rect");

			// Press inside and release outside the image: no color change and
			// the eyedropper stays active.
			h.frontend.select_tool("Pick Color");
			const auto p = pixel_pos(20, 10);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
		}
		else if (frame == 15)
		{
			const auto p = pixel_pos(20, 10);
			push_left(window, p.first, p.second, true);
		}
		else if (frame == 16)
		{
			const int x = canvas_x + 280;
			const int y = canvas_y + 200;
			warp(window, x, y);
			push_motion(window, x, y, SDL_BUTTON_LMASK);
		}
		else if (frame == 17)
		{
			const int x = canvas_x + 280;
			const int y = canvas_y + 200;
			push_left(window, x, y, false);
		}
		else if (frame == 19)
		{
			CHECK_EQ(app.foreground_index(), 2);
			CHECK(app.activeButton() == "Pick Color");

			// Both buttons cancel the pick without sampling.
			const auto p = pixel_pos(20, 10);
			warp(window, p.first, p.second);
			push_motion(window, p.first, p.second, 0);
			push_left(window, p.first, p.second, true);
		}
		else if (frame == 20)
		{
			const auto p = pixel_pos(20, 10);
			push_right(window, p.first, p.second, true);
		}
		else if (frame == 21)
		{
			const auto p = pixel_pos(20, 10);
			CHECK_FALSE(canvas.active_tool()->is_operating());
			push_left(window, p.first, p.second, false);
			push_right(window, p.first, p.second, false);
		}
		else if (frame == 23)
		{
			CHECK_EQ(app.foreground_index(), 2);
			CHECK(app.activeButton() == "Pick Color");

			int min_width = 0;
			int min_height = 0;
			SDL_GetWindowMinimumSize(window, &min_width, &min_height);
			CHECK(min_width > 0);
			CHECK(min_height > 0);
			SDL_SetWindowSize(window, min_width, min_height);
		}
		else if (frame == 24)
		{
			// Hovering the status bar must not report coordinates or colors
			// from the image that happens to be behind it.
			warp(window, 10, 590);
			push_motion(window, 10, 590, 0);
		}
		else if (frame == 25)
		{
			CHECK_FALSE(h.frontend.m_canvas_area.mouse_over_canvas());
			save_frame(renderer, "temp/smoke_status_min.ppm");

			bool status_visible = false;
			for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
			{
				if (candidate->Name &&
				    std::strstr(candidate->Name, "/Status_") != nullptr)
				{
					status_visible =
						candidate->Pos.y + candidate->Size.y <=
						ImGui::GetIO().DisplaySize.y + 0.5f;
				}
			}
			CHECK(status_visible);
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
