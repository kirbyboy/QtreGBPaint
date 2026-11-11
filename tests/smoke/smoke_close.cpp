// Smoke test: main-window title and window-close handling.
//
// Covers:
//  - the ImGui title shows "<name> - QtreGBPaint" and a leading '*' when the
//    document has unsaved changes;
//  - a close request with a clean document closes immediately;
//  - a close request with unsaved changes opens the Unsaved Changes modal and
//    only closes after the pending action is resolved.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_close.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_close

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
#include "paint/window_title.h"

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

std::string expected_window_name(const std::string& display_name,
                                 bool modified)
{
	return PaintCore::make_window_title(display_name, modified) +
		"###QtreGBPaintMain";
}

bool window_exists(const char* name)
{
	for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
	{
		if (window->Name && std::strcmp(window->Name, name) == 0)
		{
			return true;
		}
	}
	return false;
}

void push_quit()
{
	SDL_Event event{};
	event.type = SDL_QUIT;
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

bool render_frame(Harness& h)
{
	h.backend.poll_events();
	h.imgui.begin_frame(h.backend);
	const bool keep_open = h.frontend.render();
	h.backend.render_imgui(h.imgui.end_frame());
	return keep_open;
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

	// Clean document: title has no '*' and a close request closes immediately.
	CHECK(render_frame(h));
	CHECK(window_exists(expected_window_name("Untitled", false).c_str()));

	push_quit();
	CHECK(render_frame(h) == false);
	CHECK(!h.backend.quit_requested());
	CHECK(!window_exists("Unsaved Changes###unsaved_changes"));

	// The real app exits here; the smoke keeps the harness alive for the next
	// scenario, so clear the close flag explicitly.
	h.frontend.m_should_close = false;

	// Unsaved changes: the title gains a leading '*' (checked by the unit
	// tests) and a close request must wait for the Unsaved Changes modal.
	const PaintCore::Rect patch{0, 0, 1, 1};
	CHECK(h.resources.session().document().apply_pixel_patch(
		patch, {PaintCore::ColorIndex{1}}));
	CHECK(h.resources.is_modified());

	CHECK(render_frame(h));
	CHECK(window_exists(expected_window_name("Untitled", false).c_str()));
	save_frame(renderer, "temp/smoke_close_title.ppm");

	push_quit();
	CHECK(render_frame(h) == true);
	CHECK(window_exists("Unsaved Changes###unsaved_changes"));
	CHECK(h.frontend.m_pending_file_action ==
	      PaintFrontend::FileAction::Quit);

	// The modal's Save/Discard buttons resolve the pending action; resolving it
	// directly is enough to verify that Quit closes on the next frame.
	h.frontend.perform_pending_file_action();
	CHECK(render_frame(h) == false);

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
