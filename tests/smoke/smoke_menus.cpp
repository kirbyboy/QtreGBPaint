// Smoke test: menu structure and accelerator labels.
//
// Opens each top-level menu and captures a frame per menu. It also populates
// the recent-files list and leaves a floating selection plus clipboard content
// so the Selection entries are enabled.

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
#include "core/rect.h"
#include "core/sdl_renderer_backend.h"

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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

void push_motion(SDL_Window* window, int x, int y)
{
	SDL_WarpMouseInWindow(window, x, y);

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

void push_left(SDL_Window* window, int x, int y, bool down)
{
	push_motion(window, x, y);

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
	PaintSession& session = app.session();
	std::string error;
	REQUIRE(app.new_document(16, 16, error));

	const std::string recent_a = "temp/menu_recent_a.idx";
	const std::string recent_b = "temp/menu_recent_b.idx";
	std::remove(recent_a.c_str());
	std::remove(recent_b.c_str());
	REQUIRE(app.save_document_as(recent_a, error));
	REQUIRE(app.save_document_as(recent_b, error));
	CHECK(app.recent_files().size() == 2u);

	REQUIRE(session.begin_selection(PaintCore::Rect{0, 0, 4, 4}, 0));
	session.copy_to_clipboard();

	struct MenuEntry
	{
		const char* name;
		int x;
		const char* capture;
	};
	const MenuEntry menus[] = {
		{"File", 18, "temp/smoke_menus_file.ppm"},
		{"Edit", 56, "temp/smoke_menus_edit.ppm"},
		{"View", 100, "temp/smoke_menus_view.ppm"},
		{"Selection", 146, "temp/smoke_menus_selection.ppm"},
		{"Image", 235, "temp/smoke_menus_image.ppm"},
		{"Palette", 295, "temp/smoke_menus_palette.ppm"},
		{"Settings", 355, "temp/smoke_menus_settings.ppm"},
		{"Help", 420, "temp/smoke_menus_help.ppm"},
	};
	const int menu_count = static_cast<int>(sizeof(menus) / sizeof(menus[0]));

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		bool handled = false;
		for (int index = 0; index < menu_count; ++index)
		{
			const int base = 2 + index * 6 + (index > 0 ? 2 : 0);
			if (frame == base)
			{
				push_motion(window, menus[index].x, 24);
				handled = true;
			}
			else if (frame == base + 1)
			{
				push_left(window, menus[index].x, 24, true);
				handled = true;
			}
			else if (frame == base + 2)
			{
				push_left(window, menus[index].x, 24, false);
				handled = true;
			}
			else if (frame == base + 3)
			{
				save_frame(renderer, menus[index].capture);
				handled = true;
			}
			else if (index == 0 && frame == base + 4)
			{
				// Hover Export to open its format submenu.
				push_motion(window, 60, 130);
				handled = true;
			}
			else if (index == 0 && frame == base + 5)
			{
				// Let the submenu open on the frame after the hover.
				handled = true;
			}
			else if (index == 0 && frame == base + 6)
			{
				save_frame(renderer, "temp/smoke_menus_export.ppm");
				handled = true;
			}
			else if (frame == base + 4 + (index == 0 ? 3 : 0))
			{
				ImGui::ClosePopupToLevel(0, true);
				handled = true;
			}
		}

		if (!handled && frame == 2 + 8 + (menu_count - 1) * 6)
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

	session.cancel_floating_region();
	std::remove(recent_a.c_str());
	std::remove(recent_b.c_str());

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
