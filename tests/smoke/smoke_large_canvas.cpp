// Smoke test: a canvas wider than the GPU texture limit must render from
// 256x256 tiles instead of a single full-size texture.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_large_canvas.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_large_canvas

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "core/sdl_renderer_backend.h"
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"

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

bool read_pixel(SDL_Renderer* renderer, int x, int y, std::uint8_t rgb[3])
{
	SDL_Rect rect{x, y, 1, 1};
	if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGB24,
	                         rgb, 3) != 0)
	{
		std::cerr << "SDL_RenderReadPixels failed: " << SDL_GetError() << "\n";
		return false;
	}
	return true;
}

bool same_color(const std::uint8_t rgb[3], const PaintCore::Color& color)
{
	return rgb[0] == color.r && rgb[1] == color.g && rgb[2] == color.b;
}

PaintCore::Color border_color()
{
	const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
	return PaintCore::Color{
		static_cast<std::uint8_t>((color >> IM_COL32_R_SHIFT) & 0xFF),
		static_cast<std::uint8_t>((color >> IM_COL32_G_SHIFT) & 0xFF),
		static_cast<std::uint8_t>((color >> IM_COL32_B_SHIFT) & 0xFF),
		0xFF,
	};
}

// Every backend texture must be a tile-sized texture. Count them so the test
// can also prove the tile cache was actually used.
void inspect_textures(SDLRendererBackend& backend, int& count, int& max_dim)
{
	count = 0;
	max_dim = 0;
	for (const auto& entry : backend.m_textures)
	{
		int width = 0;
		int height = 0;
		if (entry.second == nullptr ||
		    SDL_QueryTexture(entry.second, nullptr, nullptr, &width, &height) != 0)
		{
			++g_failures;
			std::cerr << "FAIL could not query a backend texture\n";
			continue;
		}
		++count;
		max_dim = std::max(max_dim, std::max(width, height));
	}
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

	PaintSession& canvas = h.frontend.m_canvas_area.session();
	constexpr int kCanvasWidth = 17000; // > 16384 backend texture limit
	constexpr int kCanvasHeight = 64;
	constexpr int kBlock = 32;
	constexpr int kStripeWidth = 2 * PaintCore::kCanvasTileSize;

	CHECK(canvas.resize(kCanvasWidth, kCanvasHeight, 0));

	// Two full-height stripes aligned with the tile boundary at x=256, plus a
	// block at the far right edge so scrolling can be verified. The grid is
	// disabled below to sample exact pixel colors.
	std::vector<PaintCore::ColorIndex> stripes(
		static_cast<std::size_t>(kStripeWidth) * kCanvasHeight);
	for (int y = 0; y < kCanvasHeight; ++y)
	{
		for (int x = 0; x < kStripeWidth; ++x)
		{
			stripes[static_cast<std::size_t>(y) * kStripeWidth +
			        static_cast<std::size_t>(x)] =
				(x < PaintCore::kCanvasTileSize) ? 1 : 2;
		}
	}
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, kStripeWidth, kCanvasHeight}, stripes);

	std::vector<PaintCore::ColorIndex> right(
		static_cast<std::size_t>(kBlock) * kBlock, 3);
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{kCanvasWidth - kBlock, 0, kBlock, kBlock}, right);
	canvas.clear_history();
	h.frontend.m_show_grid = false;

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

	const PaintCore::Color expected_stripe_a =
		h.resources.palette().getColor(1);
	const PaintCore::Color expected_stripe_b =
		h.resources.palette().getColor(2);
	const PaintCore::Color expected_patch =
		h.resources.palette().getColor(3);

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 2)
		{
			// Dirty update inside an already-cached tile.
			std::vector<PaintCore::ColorIndex> patch(
				static_cast<std::size_t>(8) * 8, 3);
			canvas.document().apply_pixel_patch(
				PaintCore::Rect{16, 16, 8, 8}, patch);
		}
		else if (frame == 3)
		{
			int count = 0;
			int max_dim = 0;
			inspect_textures(h.backend, count, max_dim);
			std::cout << "textures=" << count << " max_dim=" << max_dim << "\n";
			CHECK(count > 0);
			CHECK(max_dim <= PaintCore::kCanvasTileSize);

			const PaintCore::Vec2f origin =
				h.frontend.m_canvas_area.m_view.origin;
			const int origin_x = static_cast<int>(origin.x);
			const int origin_y = static_cast<int>(origin.y);
			std::uint8_t rgb[3]{};

			if (read_pixel(renderer, origin_x + 8, origin_y + 8, rgb))
			{
				CHECK(same_color(rgb, expected_stripe_a));
				std::cout << "left sample " << int(rgb[0]) << ","
				          << int(rgb[1]) << "," << int(rgb[2]) << "\n";
			}
			if (read_pixel(renderer, origin_x + 18, origin_y + 18, rgb))
			{
				CHECK(same_color(rgb, expected_patch));
			}
			// Both sides of the x=256 tile boundary.
			if (read_pixel(renderer, origin_x + 250, origin_y + 16, rgb))
			{
				CHECK(same_color(rgb, expected_stripe_a));
			}
			if (read_pixel(renderer, origin_x + 260, origin_y + 16, rgb))
			{
				CHECK(same_color(rgb, expected_stripe_b));
			}
			// Blue highlight just outside the bottom image edge.
			if (read_pixel(renderer, origin_x + 8,
			               origin_y + kCanvasHeight + 1, rgb))
			{
				CHECK(same_color(rgb, border_color()));
			}
			save_frame(renderer, "temp/smoke_large_canvas_left.ppm");
		}
		else if (frame == 4)
		{
			// A palette change must refresh every cached tile, not just the
			// dirty one.
			h.resources.toggle_palette_inverted();
		}
		else if (frame == 6)
		{
			const PaintCore::Vec2f origin =
				h.frontend.m_canvas_area.m_view.origin;
			std::uint8_t rgb[3]{};
			if (read_pixel(renderer,
			               static_cast<int>(origin.x) + 8,
			               static_cast<int>(origin.y) + 8, rgb))
			{
				CHECK(same_color(rgb, h.resources.palette().getColor(1)));
				std::cout << "palette sample " << int(rgb[0]) << ","
				          << int(rgb[1]) << "," << int(rgb[2]) << "\n";
			}
			save_frame(renderer, "temp/smoke_large_canvas_palette.ppm");
		}
		else if (frame == 7)
		{
			ImGuiWindow* canvas_window = find_canvas_window();
			CHECK(canvas_window != nullptr);
			if (canvas_window)
			{
				ImGui::SetScrollX(canvas_window, canvas_window->ScrollMax.x);
				ImGui::SetScrollY(canvas_window, canvas_window->ScrollMax.y);
			}
		}
		else if (frame == 10)
		{
			int count = 0;
			int max_dim = 0;
			inspect_textures(h.backend, count, max_dim);
			std::cout << "scrolled textures=" << count
			          << " max_dim=" << max_dim << "\n";
			CHECK(count > 0);
			CHECK(count < 128);
			CHECK(max_dim <= PaintCore::kCanvasTileSize);

			const PaintCore::Vec2f origin =
				h.frontend.m_canvas_area.m_view.origin;
			const PaintCore::Rect visible =
				h.frontend.m_canvas_area.m_view.visible_image_rect(
					canvas.getWidth(), canvas.getHeight());
			std::cout << "visible=" << visible.x << "," << visible.y << " "
			          << visible.w << "x" << visible.h
			          << " origin=" << origin.x << "," << origin.y << "\n";
			CHECK(visible.right() == kCanvasWidth);

			std::uint8_t rgb[3]{};
			const int sample_x =
				static_cast<int>(origin.x) + kCanvasWidth - 16;
			const int sample_y = static_cast<int>(origin.y) + 16;
			if (read_pixel(renderer, sample_x, sample_y, rgb))
			{
				CHECK(same_color(rgb, h.resources.palette().getColor(3)));
				std::cout << "right sample " << int(rgb[0]) << ","
				          << int(rgb[1]) << "," << int(rgb[2]) << "\n";
			}
			// Blue highlight just outside the right image edge.
			if (read_pixel(renderer,
			               static_cast<int>(origin.x) + kCanvasWidth + 1,
			               static_cast<int>(origin.y) + 8, rgb))
			{
				CHECK(same_color(rgb, border_color()));
			}
			save_frame(renderer, "temp/smoke_large_canvas_right.ppm");
			running = false;
		}

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
