// Smoke test for the per-tool mouse cursor behavior.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_cursor.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_cursor

#include <SDL.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/paint_frontend.h"
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
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(motion) failed\n";
	}
}

void keep_pointer(SDL_Window* window, int x, int y)
{
	SDL_WarpMouseInWindow(window, x, y);
	push_motion(window, x, y, SDL_BUTTON_LMASK);
}

void push_button(SDL_Window* window, int x, int y, bool down)
{
	SDL_WarpMouseInWindow(window, x, y);
	push_motion(window, x, y, down ? SDL_BUTTON_LMASK : 0);

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
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(button) failed\n";
	}
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
	h.resources.addButton("Pencil", nullptr,
	                      PaintCore::CursorType::Pencil,
	                      asset_base + "/pencil_button.idx", gray);
	h.resources.addButton("Fill", nullptr,
	                      PaintCore::CursorType::Fill,
	                      asset_base + "/fill_button.idx", gray);
	h.resources.addButton("Circle", nullptr,
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/circle_button.idx", gray);

	CHECK(h.resources.buttons().size() == 4);
	CHECK(h.resources.buttons()[0].cursor == PaintCore::CursorType::Crosshair);
	CHECK(h.resources.buttons()[1].cursor == PaintCore::CursorType::Pencil);
	CHECK(h.resources.buttons()[2].cursor == PaintCore::CursorType::Fill);
	CHECK(h.resources.buttons()[3].cursor == PaintCore::CursorType::Crosshair);

	std::string cursor_error;
	CHECK(h.resources.loadCursor("Rect", asset_base + "/cursors/cross.ipx",
	                             cursor_error));
	CHECK(cursor_error.empty());

	CHECK(h.resources.loadCursor("Pencil",
	                             asset_base + "/cursors/pencil.ipx",
	                             cursor_error));
	CHECK(cursor_error.empty());

	CHECK(h.resources.loadCursor("Fill",
	                              asset_base + "/cursors/fill.ipx",
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

	const int canvas_x = 168;
	const int canvas_y = 103;

	SDL_Cursor* arrow_cursor = nullptr;
	SDL_Cursor* cross_cursor = nullptr;
	SDL_Cursor* pencil_cursor = nullptr;
	SDL_Cursor* fill_cursor = nullptr;

	h.frontend.select_tool("Rect");
	CHECK(h.resources.activeCursor() == PaintCore::CursorType::Crosshair);

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 1)
		{
			// The real mouse is outside the canvas, so this is the default.
			arrow_cursor = SDL_GetCursor();
			CHECK(arrow_cursor != nullptr);

			// Hold the button so the synthetic position is not replaced by
			// ImGui's global-mouse fallback while the test runs.
			push_button(window, canvas_x, canvas_y, true);
		}
		else if (frame == 2)
		{
			keep_pointer(window, canvas_x, canvas_y);
			cross_cursor = SDL_GetCursor();
			CHECK(cross_cursor != nullptr);
			CHECK(cross_cursor != arrow_cursor);

			h.frontend.select_tool("Pencil");
			CHECK(h.resources.activeCursor() == PaintCore::CursorType::Pencil);
		}
		else if (frame == 3)
		{
			keep_pointer(window, canvas_x, canvas_y);
			pencil_cursor = SDL_GetCursor();
			CHECK(pencil_cursor != nullptr);
			CHECK(pencil_cursor != cross_cursor);

			h.frontend.select_tool("Fill");
			CHECK(h.resources.activeCursor() == PaintCore::CursorType::Fill);
		}
		else if (frame == 4)
		{
			keep_pointer(window, canvas_x, canvas_y);
			fill_cursor = SDL_GetCursor();
			CHECK(fill_cursor != nullptr);
			CHECK(fill_cursor != pencil_cursor);

			h.frontend.select_tool("Circle");
			CHECK(h.resources.activeCursor() == PaintCore::CursorType::Crosshair);
		}
		else if (frame == 5)
		{
			keep_pointer(window, canvas_x, canvas_y);
			SDL_Cursor* circle_cursor = SDL_GetCursor();
			CHECK(circle_cursor != nullptr);
			CHECK(circle_cursor != fill_cursor);
			CHECK(circle_cursor == cross_cursor);

			// Release outside the canvas: the default cursor must return.
			push_button(window, 20, 14, false);
		}
		else if (frame == 6)
		{
			CHECK(SDL_GetCursor() == arrow_cursor);
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
