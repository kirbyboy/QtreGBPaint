// Smoke test: palette transparency across tools and rendering.
//
// Covers:
//   - 8bpp exposes all 256 indices and index 255 is a regular visible swatch.
//   - Marking the foreground transparent gives it alpha 0 and does not touch
//     the document or its history.
//   - Fill and Pencil paint the transparent index; the coverage mask keeps it
//     distinct from untouched pixels.
//   - The canvas checkerboard shows through transparent pixels, including the
//     live Pencil preview.
//   - Undo/redo, eraser with a transparent background and save/open preserve
//     raw indices; opening resets the transparency.
//
// Captures:
//   temp/smoke_transparency_fill.ppm
//   temp/smoke_transparency_preview.ppm

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
#include "tools/eraser_tool.h"
#include "tools/fill_tool.h"
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

	PaintSession& canvas() { return frontend.m_canvas_area.session(); }
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

bool is_checkerboard(Uint8 r, Uint8 g, Uint8 b)
{
	return (r == 64 && g == 64 && b == 64) ||
	       (r == 192 && g == 192 && b == 192);
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
	h.resources.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                      PaintCore::CursorType::Fill,
	                      asset_base + "/fill_button.idx", gray);
	h.resources.addButton("Pencil", std::make_shared<PencilTool>(1, 2),
	                      PaintCore::CursorType::Pencil,
	                      asset_base + "/pencil_button.idx", gray);
	h.resources.addButton("Eraser", std::make_shared<EraserTool>(1, 2),
	                      PaintCore::CursorType::Eraser,
	                      asset_base + "/eraser_button.idx", gray);

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

	std::string error;
	REQUIRE(h.resources.new_document(32, 32, 8, error));
	CHECK(error.empty());

	// 8bpp exposes all 256 indices; the last visible swatch is index 255.
	CHECK_EQ(h.resources.palette_usable_count(), 256);
	CHECK_EQ(h.resources.palette_visual_count(), 32);
	CHECK_EQ(h.resources.palette_visual_index(31), 255);
	CHECK_EQ(h.resources.foreground_index(), 255);
	CHECK_EQ(h.resources.background_index(), 0);
	CHECK_FALSE(h.resources.has_transparent_color());
	CHECK_FALSE(h.resources.is_modified());

	CHECK(h.resources.set_transparent_foreground());
	REQUIRE(h.resources.transparent_index().has_value());
	CHECK_EQ(*h.resources.transparent_index(), 255);
	CHECK(h.resources.palette().is_transparent(255));
	CHECK_EQ(h.resources.palette().getColor(255).a, 0);
	CHECK_FALSE(h.resources.is_modified());
	CHECK_FALSE(h.resources.can_undo());
	CHECK_FALSE(h.resources.set_transparent_color(254));

	h.frontend.m_zoom = 4;
	h.frontend.m_show_grid = false;
	h.frontend.select_tool("Fill");

	const std::string save_path = "/tmp/opencode/smoke_transparency.idx";
	int origin_x = 0;
	int origin_y = 0;
	int center_x = 0;
	int center_y = 0;
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		origin_x = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.x);
		origin_y = static_cast<int>(h.frontend.m_canvas_area.m_view.origin.y);
		center_x = origin_x + 16 * 4;
		center_y = origin_y + 16 * 4;

		if (frame == 2)
		{
			warp(window, center_x, center_y);
			push_motion(window, center_x, center_y, 0);
		}
		else if (frame == 3)
		{
			warp(window, center_x, center_y);
			push_motion(window, center_x, center_y, 0);
			push_left(window, center_x, center_y, true);
		}
		else if (frame == 4)
		{
			push_left(window, center_x, center_y, false);
		}
		else if (frame == 6)
		{
			// Fill wrote the transparent index into the canvas.
			const ImageBuffer& image = h.canvas().get_main_buffer();
			CHECK_EQ(image.get_pixel(0, 0), 255);
			CHECK_EQ(image.get_pixel(16, 16), 255);
			CHECK_EQ(image.get_pixel(31, 31), 255);
			CHECK(h.resources.is_modified());
			CHECK(h.resources.can_undo());
			CHECK(h.resources.has_transparent_color());
		}
		else if (frame == 7)
		{
			// Committed transparent pixels show the screen-space checkerboard.
			Uint8 r0 = 0, g0 = 0, b0 = 0;
			Uint8 r1 = 0, g1 = 0, b1 = 0;
			const bool ok0 = read_rgb(renderer, origin_x + 2, origin_y + 2,
			                          r0, g0, b0);
			const bool ok1 = read_rgb(renderer, origin_x + 10, origin_y + 2,
			                          r1, g1, b1);
			CHECK(ok0);
			CHECK(ok1);
			CHECK(is_checkerboard(r0, g0, b0));
			CHECK(is_checkerboard(r1, g1, b1));
			CHECK(!(r0 == r1 && g0 == g1 && b0 == b1));
			save_frame(renderer, "temp/smoke_transparency_fill.ppm");
		}
		else if (frame == 8)
		{
			CHECK(h.canvas().undo());
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(0, 0), 0);
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(31, 31), 0);
			CHECK(h.resources.has_transparent_color());
			CHECK_FALSE(h.resources.is_modified());
		}
		else if (frame == 9)
		{
			CHECK(h.canvas().redo());
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(0, 0), 255);
			CHECK(h.resources.is_modified());
		}
		else if (frame == 10)
		{
			// 255 renders opaque again; erase an opaque background block to
			// paint the transparent foreground over it later.
			h.resources.clear_transparent_color();
			CHECK_EQ(h.resources.palette().getColor(255).a, 255);
			h.frontend.select_tool("Eraser");
			warp(window, origin_x + 8 * 4 + 2, origin_y + 8 * 4 + 2);
			push_motion(window, origin_x + 8 * 4 + 2,
			            origin_y + 8 * 4 + 2, 0);
		}
		else if (frame == 11)
		{
			push_left(window, origin_x + 8 * 4 + 2,
			          origin_y + 8 * 4 + 2, true);
		}
		else if (frame == 12)
		{
			push_left(window, origin_x + 8 * 4 + 2,
			          origin_y + 8 * 4 + 2, false);
		}
		else if (frame == 13)
		{
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(8, 8), 0);
			CHECK(h.resources.set_transparent_color(255));
			CHECK(h.resources.palette().is_transparent(255));
			h.frontend.select_tool("Pencil");
			h.resources.set_foreground_index(255);
		}
		else if (frame == 14)
		{
			warp(window, origin_x + 8 * 4 + 2, origin_y + 8 * 4 + 2);
			push_motion(window, origin_x + 8 * 4 + 2,
			            origin_y + 8 * 4 + 2, 0);
		}
		else if (frame == 15)
		{
			push_left(window, origin_x + 8 * 4 + 2,
			          origin_y + 8 * 4 + 2, true);
		}
		else if (frame == 16)
		{
			// The live pencil preview paints the transparent index over the
			// old opaque pixel: the checkerboard must show during the stroke,
			// before the DrawBuffer is committed.
			const ImageBuffer& image = h.canvas().get_main_buffer();
			CHECK_EQ(image.get_pixel(8, 8), 0);
			Uint8 r = 0, g = 0, b = 0;
			const bool ok = read_rgb(renderer, origin_x + 8 * 4 + 2,
			                         origin_y + 8 * 4 + 2, r, g, b);
			CHECK(ok);
			CHECK(is_checkerboard(r, g, b));
			save_frame(renderer, "temp/smoke_transparency_preview.ppm");
		}
		else if (frame == 17)
		{
			push_left(window, origin_x + 8 * 4 + 2,
			          origin_y + 8 * 4 + 2, false);
		}
		else if (frame == 18)
		{
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(8, 8), 255);
		}
		else if (frame == 19)
		{
			// Move transparency to the background and erase with it.
			CHECK(h.resources.set_transparent_color(0));
			CHECK(h.resources.palette().is_transparent(0));
			CHECK_EQ(h.resources.palette().getColor(255).a, 255);
			h.frontend.select_tool("Eraser");
			warp(window, origin_x + 4 * 4 + 2, origin_y + 4 * 4 + 2);
			push_motion(window, origin_x + 4 * 4 + 2,
			            origin_y + 4 * 4 + 2, 0);
		}
		else if (frame == 20)
		{
			push_left(window, origin_x + 4 * 4 + 2,
			          origin_y + 4 * 4 + 2, true);
		}
		else if (frame == 21)
		{
			push_left(window, origin_x + 4 * 4 + 2,
			          origin_y + 4 * 4 + 2, false);
		}
		else if (frame == 22)
		{
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(4, 4), 0);
			Uint8 r = 0, g = 0, b = 0;
			const bool ok = read_rgb(renderer, origin_x + 4 * 4 + 2,
			                         origin_y + 4 * 4 + 2, r, g, b);
			CHECK(ok);
			CHECK(is_checkerboard(r, g, b));
		}
		else if (frame == 23)
		{
			// Clear transparency: every swatch renders opaque again, but the
			// raw pixel indices stay untouched.
			h.resources.clear_transparent_color();
			CHECK_FALSE(h.resources.has_transparent_color());
			CHECK_EQ(h.resources.palette().getColor(0).a, 255);
			CHECK_EQ(h.resources.palette().getColor(255).a, 255);
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(8, 8), 255);
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(4, 4), 0);
		}
		else if (frame == 24)
		{
			// Save with transparency set, then reopen: the .idx file keeps the
			// raw index, while opening resets the palette transparency.
			CHECK(h.resources.set_transparent_color(255));
			REQUIRE(h.resources.save_document_as(save_path, error));
			CHECK(error.empty());
		}
		else if (frame == 25)
		{
			REQUIRE(h.resources.open_document(save_path, error));
			CHECK(error.empty());
			CHECK_FALSE(h.resources.has_transparent_color());
			CHECK_EQ(h.resources.session().getBitsPerPixel(), 8);
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(8, 8), 255);
			CHECK_EQ(h.canvas().get_main_buffer().get_pixel(4, 4), 0);
		}
		else if (frame == 26)
		{
			// A floating selection with a transparent hole fill must cover the
			// old canvas pixels with the checkerboard, not reveal them.
			CHECK(h.resources.set_transparent_color(0));
			REQUIRE(h.canvas().begin_selection({0, 0, 4, 4}, 0, false));
			REQUIRE(h.canvas().move_floating_region(8, 8));
			CHECK(h.canvas().has_floating_region());
		}
		else if (frame == 27)
		{
			Uint8 r = 0, g = 0, b = 0;
			const bool ok = read_rgb(renderer, origin_x + 6, origin_y + 6,
			                         r, g, b);
			CHECK(ok);
			CHECK(is_checkerboard(r, g, b));
			h.canvas().cancel_floating_region();
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
