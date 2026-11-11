// Smoke test for the zoom-out scroll clamp at the canvas edges.
// Reproduces: zoom 64 + scroll to max, then zoom out. Without forcing the new
// content size before BeginChild, the first frame uses stale scroll and shows a
// blank strip. Build from the repository root like the other smokes.

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

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

ImGuiWindow* find_canvas_window()
{
	for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
	{
		if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
		{
			return candidate;
		}
	}
	return nullptr;
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

bool sample_canvas_bottom_right(SDL_Renderer* renderer, int inset,
                                std::uint8_t rgb[3], ImVec2& point)
{
	ImGuiWindow* window = find_canvas_window();
	if (!window)
	{
		return false;
	}

	const ImRect clip = window->InnerClipRect;
	point = ImVec2(clip.Max.x - inset, clip.Max.y - inset);
	if (point.x < clip.Min.x || point.y < clip.Min.y)
	{
		return false;
	}

	int width = 0;
	int height = 0;
	SDL_GetRendererOutputSize(renderer, &width, &height);
	std::vector<std::uint8_t> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
	if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24,
	                         pixels.data(), width * 3) != 0)
	{
		return false;
	}

	const int x = static_cast<int>(point.x);
	const int y = static_cast<int>(point.y);
	if (x < 0 || y < 0 || x >= width || y >= height)
	{
		return false;
	}
	const std::size_t index =
		(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
		 static_cast<std::size_t>(x)) * 3;
	rgb[0] = pixels[index];
	rgb[1] = pixels[index + 1];
	rgb[2] = pixels[index + 2];
	return true;
}

void check_canvas_color(SDL_Renderer* renderer, const char* stage)
{
	std::uint8_t rgb[3] = {0, 0, 0};
	ImVec2 point;
	if (!sample_canvas_bottom_right(renderer, 24, rgb, point))
	{
		++g_failures;
		std::cerr << "FAIL " << stage << ": could not sample canvas\n";
		return;
	}
	std::cout << stage << " sample at " << point.x << "," << point.y
	          << " = " << int(rgb[0]) << "," << int(rgb[1]) << ","
	          << int(rgb[2]) << "\n";
	CHECK(rgb[0] == 224);
	CHECK(rgb[1] == 248);
	CHECK(rgb[2] == 208);
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

	h.frontend.m_show_grid = false;
	h.frontend.m_zoom = 64;

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 3)
		{
			if (ImGuiWindow* canvas_window = find_canvas_window())
			{
				ImGui::SetScrollX(canvas_window, canvas_window->ScrollMax.x);
				ImGui::SetScrollY(canvas_window, canvas_window->ScrollMax.y);
			}
		}
		else if (frame == 6)
		{
			check_canvas_color(renderer, "zoom 64 max scroll");
			save_frame(renderer, "temp/smoke_zoom_edge_before.ppm");
		}
		else if (frame == 8)
		{
			check_canvas_color(renderer, "first frame after zoom 24");
			save_frame(renderer, "temp/smoke_zoom_edge_after.ppm");
		}
		else if (frame == 9)
		{
			check_canvas_color(renderer, "second frame after zoom 24");
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		if (frame == 7)
		{
			// Simulate the zoom slider, which is rendered in the palette after
			// the canvas and therefore changes m_zoom at the end of the frame.
			h.frontend.m_zoom = 24;
		}
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
