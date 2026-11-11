// Smoke test with injected SDL events for the selection/floating region.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_selection.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_selection

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

// Test access to the frontend's internal Canvas (the project is not modified).
#define private public
#include "paint/paint_frontend.h"
#undef private

#include "paint/image_buffer.h"
#include "tools/base_tool.h"
#include "tools/selection_tool.h"
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

#define CHECK_FALSE(expr) CHECK(!(expr))
#define CHECK_EQ(a, b) CHECK((a) == (b))

// The canvas origin depends on the window layout, so injected coordinates are
// derived from the real ViewTransform every frame.
int canvas_x = 0;
int canvas_y = 0;
int expected_paste_x = 0;
int expected_paste_y = 0;

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
		std::cerr << "SDL_RenderReadPixels failed: " << SDL_GetError() << "\n";
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

PaintCore::Rect image_rect(int x, int y, int w, int h)
{
	return PaintCore::Rect{x, y, w, h};
}

bool rect_eq(const PaintCore::Rect& a, const PaintCore::Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

int run()
{
	// Software rendering makes SDL_RenderReadPixels deterministic for the
	// screenshots; the app itself uses the platform renderer.
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
	h.resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/selection_button.idx", gray);
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);

	std::string cursor_error;
	CHECK(h.resources.loadCursor(PaintCore::CursorType::Move,
	                             asset_base + "/cursors/move.ipx",
	                             cursor_error));
	CHECK(h.resources.loadOptionIcon("selection.normal",
	                                 asset_base + "/selection_options.idx",
	                                 0, 0, 60, 16));
	CHECK(h.resources.loadOptionIcon("selection.transparent",
	                                 asset_base + "/selection_options.idx",
	                                 0, 16, 60, 16));

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

	PaintSession& canvas = h.canvas();
	std::vector<PaintCore::ColorIndex> seed(
		static_cast<std::size_t>(20) * 20);
	for (int y = 10; y < 30; ++y)
	{
		for (int x = 10; x < 30; ++x)
		{
			seed[static_cast<std::size_t>(y - 10) * 20 +
			     static_cast<std::size_t>(x - 10)] =
				((x + y) % 7 == 0) ? 3 : 2;
		}
	}
	canvas.document().apply_pixel_patch(
		PaintCore::Rect{10, 10, 20, 20}, seed);
	canvas.clear_history();

	// The smoke checks pixel indices assuming the classic dark fg / light bg.
	h.resources.set_foreground_index(3);
	h.resources.set_background_index(0);

	h.frontend.select_tool("Selection");
	CHECK(h.resources.activeButton() == "Selection");
	CHECK(h.resources.activeCursor() == PaintCore::CursorType::Crosshair);

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
			push_motion(window, canvas_x + 10, canvas_y + 10, 0);
		}
		else if (frame == 3)
		{
			push_left(window, canvas_x + 10, canvas_y + 10, true);
		}
		else if (frame >= 4 && frame <= 10)
		{
			const int t = frame - 4;
			push_motion(window, canvas_x + 10 + 19 * t / 6,
			            canvas_y + 10 + 19 * t / 6, SDL_BUTTON_LMASK);
		}
		else if (frame == 11)
		{
			push_left(window, canvas_x + 29, canvas_y + 29, false);
		}
		else if (frame == 16)
		{
			push_motion(window, canvas_x + 15, canvas_y + 15, 0);
		}
		else if (frame == 17)
		{
			push_left(window, canvas_x + 15, canvas_y + 15, true);
		}
		else if (frame >= 18 && frame <= 24)
		{
			const int t = frame - 18;
			push_motion(window, canvas_x + 15 + 20 * t / 6,
			            canvas_y + 15 + 10 * t / 6, SDL_BUTTON_LMASK);
		}
		else if (frame == 25)
		{
			push_left(window, canvas_x + 35, canvas_y + 25, false);
		}
		else if (frame == 30)
		{
			push_motion(window, canvas_x + 100, canvas_y + 100, 0);
		}
		else if (frame == 31)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, true);
		}
		else if (frame == 32)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, false);
		}
		else if (frame == 40)
		{
			push_motion(window, canvas_x + 50, canvas_y + 50, 0);
		}
		else if (frame == 41)
		{
			push_left(window, canvas_x + 50, canvas_y + 50, true);
		}
		else if (frame >= 42 && frame <= 46)
		{
			const int t = frame - 42;
			push_motion(window, canvas_x + 50 + 10 * t / 4,
			            canvas_y + 50 + 10 * t / 4, SDL_BUTTON_LMASK);
		}
		else if (frame == 47)
		{
			push_left(window, canvas_x + 60, canvas_y + 60, false);
		}
		else if (frame == 52)
		{
			// Right-click cancels while the region is not being dragged.
			push_motion(window, canvas_x + 55, canvas_y + 55, 0);
		}
		else if (frame == 53)
		{
			push_button(window, canvas_x + 55, canvas_y + 55,
			            SDL_BUTTON_RIGHT, true);
		}
		else if (frame == 54)
		{
			push_button(window, canvas_x + 55, canvas_y + 55,
			            SDL_BUTTON_RIGHT, false);
		}
		else if (frame == 62)
		{
			push_motion(window, canvas_x + 70, canvas_y + 70, 0);
		}
		else if (frame == 63)
		{
			push_left(window, canvas_x + 70, canvas_y + 70, true);
		}
		else if (frame >= 64 && frame <= 68)
		{
			const int t = frame - 64;
			push_motion(window, canvas_x + 70 + 10 * t / 4,
			            canvas_y + 70 + 10 * t / 4, SDL_BUTTON_LMASK);
		}
		else if (frame == 69)
		{
			push_left(window, canvas_x + 80, canvas_y + 80, false);
		}
		else if (frame == 74)
		{
			push_motion(window, canvas_x + 75, canvas_y + 75, 0);
		}
		else if (frame == 75)
		{
			push_left(window, canvas_x + 75, canvas_y + 75, true);
		}
		else if (frame == 76)
		{
			push_motion(window, canvas_x + 85, canvas_y + 75,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 77)
		{
			push_left(window, canvas_x + 85, canvas_y + 75, false);
		}
		else if (frame == 82)
		{
			h.frontend.select_tool("Rect");
		}
		else if (frame == 90)
		{
			// Undo the committed first selection.
			canvas.undo();
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 12)
		{
			CHECK(canvas.has_floating_region());
			CHECK(rect_eq(canvas.floating_region().rect,
			              image_rect(10, 10, 20, 20)));
			CHECK(rect_eq(canvas.floating_region().hole,
			              image_rect(10, 10, 20, 20)));
			CHECK_EQ(canvas.floating_region().hole_fill, 0);
			save_frame(renderer, "temp/smoke_selection_created.ppm");
		}
		else if (frame == 26)
		{
			CHECK(rect_eq(canvas.floating_region().rect,
			              image_rect(30, 20, 20, 20)));
			// While the region floats the canvas keeps the original pixels;
			// the hole fill is only part of the overlay.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 10), 2);
			save_frame(renderer, "temp/smoke_selection_moved.ppm");
		}
		else if (frame == 33)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(10, 10), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(30, 20), 2);
			CHECK(canvas.can_undo());
		}
		else if (frame == 34)
		{
			save_frame(renderer, "temp/smoke_selection_committed.ppm");
		}
		else if (frame == 48)
		{
			CHECK(canvas.has_floating_region());
		}
		else if (frame == 55)
		{
			CHECK_FALSE(canvas.has_floating_region());
		}
		else if (frame == 70)
		{
			CHECK(canvas.has_floating_region());
			CHECK(rect_eq(canvas.floating_region().rect,
			              image_rect(70, 70, 11, 11)));
		}
		else if (frame == 78)
		{
			CHECK(canvas.has_floating_region());
			CHECK(rect_eq(canvas.floating_region().rect,
			              image_rect(80, 70, 11, 11)));
		}
		else if (frame == 83)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK(h.resources.activeButton() == "Rect");
			// The selected area was empty and the hole fill is the background
			// color, so this commit does not change pixels.
			CHECK_EQ(canvas.get_main_buffer().get_pixel(70, 70), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(80, 70), 0);
		}
		else if (frame == 84)
		{
			save_frame(renderer, "temp/smoke_selection_tool_switch.ppm");
		}
		else if (frame == 91)
		{
			CHECK_EQ(canvas.get_main_buffer().get_pixel(70, 70), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(80, 70), 0);
			CHECK(canvas.can_redo());
		}
		else if (frame == 94)
		{
			CHECK(canvas.begin_selection(image_rect(20, 20, 4, 4), 0));
		}
		else if (frame == 96)
		{
			push_motion(window, 200, 24, 0);
		}
		else if (frame == 97)
		{
			push_left(window, 200, 24, true);
		}
		else if (frame == 98)
		{
			push_left(window, 200, 24, false);
		}
		else if (frame == 100)
		{
			save_frame(renderer, "temp/smoke_selection_menu.ppm");
		}
		else if (frame == 101)
		{
			push_motion(window, 180, 145, 0);
		}
		else if (frame == 102)
		{
			push_left(window, 180, 145, true);
		}
		else if (frame == 103)
		{
			push_left(window, 180, 145, false);
		}
		else if (frame == 104)
		{
			CHECK(canvas.has_floating_region());
			// (20,20) is index 2 and (20,22) is index 3 in the DMG pattern;
			// 2bpp inversion maps 2 -> 1 and 3 -> 0.
			CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 1);
			CHECK_EQ(canvas.floating_region().content().get_pixel(0, 2), 0);
			save_frame(renderer, "temp/smoke_selection_inverted.ppm");
		}
		else if (frame == 106)
		{
			push_motion(window, canvas_x + 100, canvas_y + 100, 0);
		}
		else if (frame == 107)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, true);
		}
		else if (frame == 108)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, false);
		}
		else if (frame == 109)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 20), 1);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 22), 0);
			CHECK(canvas.can_undo());
			save_frame(renderer, "temp/smoke_selection_invert_committed.ppm");
		}
		else if (frame == 111)
		{
			CHECK(canvas.begin_selection(image_rect(20, 20, 4, 4), 0));
		}
		else if (frame == 113)
		{
			push_motion(window, 200, 24, 0);
		}
		else if (frame == 114)
		{
			push_left(window, 200, 24, true);
		}
		else if (frame == 115)
		{
			push_left(window, 200, 24, false);
		}
		else if (frame == 116)
		{
			push_motion(window, 180, 52, 0);
		}
		else if (frame == 117)
		{
			push_left(window, 180, 52, true);
		}
		else if (frame == 118)
		{
			push_left(window, 180, 52, false);
		}
		else if (frame == 119)
		{
			CHECK(canvas.has_floating_region());
			CHECK(canvas.has_clipboard());
			CHECK_EQ(canvas.clipboard().get_pixel(0, 0), 1);
			CHECK_EQ(canvas.clipboard().get_pixel(0, 2), 0);
		}
		else if (frame == 121)
		{
			push_motion(window, 200, 24, 0);
		}
		else if (frame == 122)
		{
			push_left(window, 200, 24, true);
		}
		else if (frame == 123)
		{
			push_left(window, 200, 24, false);
		}
		else if (frame == 124)
		{
			push_motion(window, 180, 69, 0);
		}
		else if (frame == 125)
		{
			push_left(window, 180, 69, true);
		}
		else if (frame == 126)
		{
			push_left(window, 180, 69, false);
		}
		else if (frame == 127)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK(canvas.has_clipboard());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 20), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(20, 22), 0);
			CHECK(canvas.can_undo());
			save_frame(renderer, "temp/smoke_selection_cut.ppm");
		}
		else if (frame == 129)
		{
			push_motion(window, 200, 24, 0);
		}
		else if (frame == 130)
		{
			push_left(window, 200, 24, true);
		}
		else if (frame == 131)
		{
			push_left(window, 200, 24, false);
		}
		else if (frame == 132)
		{
			push_motion(window, 180, 86, 0);
		}
		else if (frame == 133)
		{
			push_left(window, 180, 86, true);
		}
		else if (frame == 134)
		{
			push_left(window, 180, 86, false);
		}
		else if (frame == 135)
		{
			CHECK(canvas.has_floating_region());
			CHECK(rect_eq(canvas.floating_region().rect,
			              image_rect(0, 0, 4, 4)));
			CHECK_EQ(canvas.floating_region().content().get_pixel(0, 0), 1);
			save_frame(renderer, "temp/smoke_selection_paste.ppm");
		}
		else if (frame == 137)
		{
			push_motion(window, canvas_x + 100, canvas_y + 100, 0);
		}
		else if (frame == 138)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, true);
		}
		else if (frame == 139)
		{
			push_left(window, canvas_x + 100, canvas_y + 100, false);
		}
		else if (frame == 140)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 1);
			CHECK(canvas.can_undo());
		}
		else if (frame == 142)
		{
			// Paste must appear at the top-left visible image pixel, even when
			// zoomed into a far corner.
			h.frontend.m_zoom = 8;
		}
		else if (frame == 144)
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
				ImGui::SetScrollX(canvas_window, canvas_window->ScrollMax.x);
				ImGui::SetScrollY(canvas_window, canvas_window->ScrollMax.y);
			}
		}
		else if (frame == 146)
		{
			const PaintCore::Rect visible =
				h.frontend.m_canvas_area.m_view.visible_image_rect(
					canvas.getWidth(), canvas.getHeight());
			CHECK(visible.w > 0);
			CHECK(visible.h > 0);
			expected_paste_x = visible.x;
			expected_paste_y = visible.y;
			std::cout << "paste visible origin " << expected_paste_x << ","
			          << expected_paste_y << "\n";
			push_motion(window, 200, 24, 0);
		}
		else if (frame == 147)
		{
			push_left(window, 200, 24, true);
		}
		else if (frame == 148)
		{
			push_left(window, 200, 24, false);
		}
		else if (frame == 149)
		{
			push_motion(window, 180, 86, 0);
		}
		else if (frame == 150)
		{
			push_left(window, 180, 86, true);
		}
		else if (frame == 151)
		{
			push_left(window, 180, 86, false);
		}
		else if (frame == 152)
		{
			CHECK(canvas.has_floating_region());
			CHECK_EQ(canvas.floating_region().rect.x, expected_paste_x);
			CHECK_EQ(canvas.floating_region().rect.y, expected_paste_y);
			CHECK_EQ(canvas.floating_region().rect.w, 4);
			CHECK_EQ(canvas.floating_region().rect.h, 4);
			save_frame(renderer, "temp/smoke_selection_paste_visible.ppm");
			canvas.cancel_floating_region();
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
