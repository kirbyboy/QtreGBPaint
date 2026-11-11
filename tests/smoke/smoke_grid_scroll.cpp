// Diagnostic smoke test: grid + large canvas + scroll.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_grid_scroll.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_grid_scroll

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

namespace
{
void save_frame(SDL_Renderer* renderer, const char* path)
{
	int width = 0;
	int height = 0;
	SDL_GetRendererOutputSize(renderer, &width, &height);
	if (width <= 0 || height <= 0)
	{
		return;
	}

	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
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
};

int run()
{
	Harness h;
	h.imgui.init();
	h.backend.init();

	PaintSession& canvas = h.frontend.m_canvas_area.session();
	canvas.resize(128, 128, 0);
	constexpr int kSize = 128;
	std::vector<PaintCore::ColorIndex> pixels(
		static_cast<std::size_t>(kSize) * kSize);
	for (int y = 0; y < kSize; ++y)
	{
		for (int x = 0; x < kSize; ++x)
		{
			pixels[static_cast<std::size_t>(y) * kSize +
			       static_cast<std::size_t>(x)] =
				((x + y) % 2 == 0) ? 1 : 2;
		}
	}
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, kSize, kSize}, pixels);
	canvas.clear_history();
	h.frontend.m_zoom = 8;
	h.frontend.m_show_grid = true;

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

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 1)
		{
			ImGuiWindow* canvas_window = nullptr;
			for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
			{
				if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
				{
					canvas_window = candidate;
					break;
				}
			}
			if (canvas_window)
			{
				std::cerr << "scrollmax@1=" << canvas_window->ScrollMax.x
				          << "," << canvas_window->ScrollMax.y << "\n";
			}
		}
		else if (frame == 3)
		{
			ImGuiWindow* canvas_window = nullptr;
			for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
			{
				if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
				{
					canvas_window = candidate;
					break;
				}
			}
			if (canvas_window)
			{
				std::cerr << "scrollmax@3=" << canvas_window->ScrollMax.x
				          << "," << canvas_window->ScrollMax.y << "\n";
				ImGui::SetScrollX(canvas_window, canvas_window->ScrollMax.x);
				ImGui::SetScrollY(canvas_window, canvas_window->ScrollMax.y);
			}
		}
		else if (frame == 5)
		{
			ImGuiWindow* canvas_window = nullptr;
			for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
			{
				if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
				{
					canvas_window = candidate;
					break;
				}
			}
			if (canvas_window)
			{
				std::cerr << "scroll@5=" << canvas_window->Scroll.x
				          << "," << canvas_window->Scroll.y
				          << " max=" << canvas_window->ScrollMax.x
				          << "," << canvas_window->ScrollMax.y << "\n";
			}
			save_frame(renderer, "temp/smoke_grid_scroll.ppm");
		}
		else if (frame == 6)
		{
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

	std::cout << "SMOKE OK\n";
	return 0;
}
}

int main()
{
	return run();
}
