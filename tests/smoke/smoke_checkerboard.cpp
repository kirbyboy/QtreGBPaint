// Smoke test: tiled transparency checkerboard.
//
// The checkerboard is drawn with a 256x256 texture instead of one rect per
// cell. This test renders a transparent 256x256 document at zoom 16, so the
// visible checker area spans several texture tiles, and checks that the
// pattern stays anchored and continuous across the tile boundary.
//
// Capture: temp/smoke_checkerboard.ppm

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
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

#define REQUIRE(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
			return 1; \
		} \
	} while (0)

void push_motion(SDL_Window* window, int x, int y)
{
	SDL_Event event{};
	event.type = SDL_MOUSEMOTION;
	event.motion.timestamp = SDL_GetTicks();
	event.motion.windowID = SDL_GetWindowID(window);
	event.motion.which = 0;
	event.motion.state = 0;
	event.motion.x = x;
	event.motion.y = y;
	SDL_PushEvent(&event);
}

bool read_rgb(SDL_Renderer* renderer, int x, int y,
              Uint8& r, Uint8& g, Uint8& b)
{
	SDL_Rect rect{x, y, 1, 1};
	std::uint8_t pixel[3]{};
	if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGB24,
	                         pixel, 3) != 0)
	{
		return false;
	}
	r = pixel[0];
	g = pixel[1];
	b = pixel[2];
	return true;
}

bool is_light(Uint8 r, Uint8 g, Uint8 b)
{
	return r == 192 && g == 192 && b == 192;
}

bool is_dark(Uint8 r, Uint8 g, Uint8 b)
{
	return r == 64 && g == 64 && b == 64;
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
	h.resources.addButton("Pencil", std::make_shared<PencilTool>(1, 2),
	                      PaintCore::CursorType::Pencil,
	                      asset_base + "/pencil_button.idx",
	                      h.resources.iconPalette());

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
	REQUIRE(window != nullptr);
	SDL_Renderer* renderer = SDL_GetRenderer(window);
	REQUIRE(renderer != nullptr);

	std::string error;
	REQUIRE(h.resources.new_document(256, 256, 2, error));
	CHECK(error.empty());

	// The whole canvas starts filled with background index 0; marking it
	// transparent makes the checkerboard visible under the full image.
	REQUIRE(h.resources.set_transparent_color(0));
	CHECK(h.resources.has_transparent_color());

	h.frontend.m_zoom = 16;
	h.frontend.m_show_grid = false;

	const int frame_ms = 1000 / 60;
	int frame = 0;
	bool running = true;
	int origin_x = 0;
	int origin_y = 0;

	while (running)
	{
		const PaintCore::ViewTransform& view = h.frontend.m_canvas_area.m_view;
		origin_x = static_cast<int>(view.origin.x);
		origin_y = static_cast<int>(view.origin.y);

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 3)
		{
			REQUIRE(h.frontend.m_canvas_area.m_checkerboard.valid());
			REQUIRE(h.frontend.m_checkerboard.valid());

			// The image origin is the center of cell (0,0), which is light.
			// Each 8 px step alternates the cell parity. 256 px is exactly 32
			// cells, so the parity repeats across texture tiles.
			struct Sample
			{
				int x;
				int y;
				bool light;
			};
			const Sample samples[] = {
				{0, 0, true},
				{8, 0, false},
				{16, 0, true},
				{31 * 8, 0, false},
				{32 * 8, 0, true},
				{33 * 8, 0, false},
				{0, 32 * 8, true},
				{8, 32 * 8, false},
			};

			for (const Sample& sample : samples)
			{
				Uint8 r = 0;
				Uint8 g = 0;
				Uint8 b = 0;
				const bool ok = read_rgb(renderer, origin_x + sample.x,
				                         origin_y + sample.y, r, g, b);
				CHECK(ok);
				CHECK(sample.light ? is_light(r, g, b) : is_dark(r, g, b));
				if (!ok || (sample.light ? !is_light(r, g, b)
				                        : !is_dark(r, g, b)))
				{
					std::cerr << "sample " << sample.x << "," << sample.y
					          << " got " << static_cast<int>(r) << ","
					          << static_cast<int>(g) << ","
					          << static_cast<int>(b) << "\n";
				}
			}

			save_frame(renderer, "temp/smoke_checkerboard.ppm");
			running = false;
		}

		push_motion(window, 5, 5);
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
