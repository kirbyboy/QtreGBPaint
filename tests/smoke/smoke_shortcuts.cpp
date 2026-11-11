// Smoke test: Ctrl+N / Ctrl+O / Ctrl+S global shortcuts.
//
// Uses real SDL key events through the frontend. Verifies that the shortcuts
// open the same dialogs/popups as the File menu and that modified documents
// still go through the Unsaved Changes flow.

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

#include "ImGuiFileDialog.h"

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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

void push_key(SDL_Window* window, SDL_Keycode key, bool down, Uint16 mod)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.timestamp = SDL_GetTicks();
	event.key.windowID = SDL_GetWindowID(window);
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = 0;
	event.key.keysym.sym = key;
	event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
	event.key.keysym.mod = mod;
	SDL_PushEvent(&event);
}

// Four phases: Ctrl down, key down, key up, Ctrl up.
void push_ctrl_combo(SDL_Window* window, SDL_Keycode key, int phase)
{
	switch (phase)
	{
		case 0:
			push_key(window, SDLK_LCTRL, true, KMOD_CTRL);
			break;
		case 1:
			push_key(window, key, true, KMOD_CTRL);
			break;
		case 2:
			push_key(window, key, false, KMOD_CTRL);
			break;
		case 3:
			push_key(window, SDLK_LCTRL, false, 0);
			break;
		default:
			break;
	}
}

// Six phases: Ctrl down, Shift down, key down/up, Shift up, Ctrl up.
void push_ctrl_shift_combo(SDL_Window* window, SDL_Keycode key, int phase)
{
	switch (phase)
	{
		case 0:
			push_key(window, SDLK_LCTRL, true, KMOD_CTRL);
			break;
		case 1:
			push_key(window, SDLK_LSHIFT, true, KMOD_CTRL | KMOD_SHIFT);
			break;
		case 2:
			push_key(window, key, true, KMOD_CTRL | KMOD_SHIFT);
			break;
		case 3:
			push_key(window, key, false, KMOD_CTRL | KMOD_SHIFT);
			break;
		case 4:
			push_key(window, SDLK_LSHIFT, false, KMOD_CTRL);
			break;
		case 5:
			push_key(window, SDLK_LCTRL, false, 0);
			break;
		default:
			break;
	}
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

void close_all_popups()
{
	ImGui::ClosePopupToLevel(0, true);
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

	PaintBackend& app = h.resources;
	PaintSession& session = app.session();
	std::string error;
	if (!app.new_document(8, 8, error))
	{
		std::cerr << "new_document failed: " << error << "\n";
		return 2;
	}

	const std::string path = "temp/smoke_shortcuts.idx";
	std::remove(path.c_str());

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		// Ctrl+S without a path opens Save As.
		if (frame >= 2 && frame <= 5)
		{
			push_ctrl_combo(window, SDLK_s, frame - 2);
		}
		// Ctrl+N opens the New Image modal.
		else if (frame >= 8 && frame <= 11)
		{
			push_ctrl_combo(window, SDLK_n, frame - 8);
		}
		// Ctrl+O opens the Open dialog.
		else if (frame >= 14 && frame <= 17)
		{
			push_ctrl_combo(window, SDLK_o, frame - 14);
		}
		// Modified document + Ctrl+N asks about unsaved changes.
		else if (frame == 20)
		{
			CHECK(session.document().apply_pixel_patch(
				PaintCore::Rect{0, 0, 1, 1}, {1}));
			CHECK(app.is_modified());
		}
		else if (frame >= 22 && frame <= 25)
		{
			push_ctrl_combo(window, SDLK_n, frame - 22);
		}
		// Save the document, modify it again and save with Ctrl+S.
		else if (frame == 28)
		{
			h.frontend.save_to_path(path, false);
			CHECK(!app.is_modified());
			CHECK(session.document().apply_pixel_patch(
				PaintCore::Rect{1, 1, 1, 1}, {2}));
			CHECK(app.is_modified());
		}
		else if (frame >= 30 && frame <= 33)
		{
			push_ctrl_combo(window, SDLK_s, frame - 30);
		}
		// Ctrl+Shift+S always opens Save As.
		else if (frame >= 40 && frame <= 45)
		{
			push_ctrl_shift_combo(window, SDLK_s, frame - 40);
		}
		// Ctrl+G toggles the grid.
		else if (frame >= 48 && frame <= 51)
		{
			push_ctrl_combo(window, SDLK_g, frame - 48);
		}
		else if (frame >= 54 && frame <= 57)
		{
			push_ctrl_combo(window, SDLK_g, frame - 54);
		}
		// Ctrl+E opens Properties.
		else if (frame >= 60 && frame <= 63)
		{
			push_ctrl_combo(window, SDLK_e, frame - 60);
		}
		// Ctrl+C / Ctrl+V / Ctrl+X use the existing clipboard actions.
		else if (frame == 66)
		{
			CHECK(session.begin_selection(PaintCore::Rect{0, 0, 3, 3}, 0));
		}
		else if (frame >= 67 && frame <= 70)
		{
			push_ctrl_combo(window, SDLK_c, frame - 67);
		}
		else if (frame >= 72 && frame <= 75)
		{
			push_ctrl_combo(window, SDLK_v, frame - 72);
		}
		else if (frame == 78)
		{
			CHECK(session.begin_selection(PaintCore::Rect{0, 0, 3, 3}, 0));
		}
		else if (frame >= 79 && frame <= 82)
		{
			push_ctrl_combo(window, SDLK_x, frame - 79);
		}
		else if (frame == 86)
		{
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 3)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("SaveImage"));
		}
		else if (frame == 6)
		{
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 9)
		{
			CHECK(window_exists("New Image"));
		}
		else if (frame == 12)
		{
			close_all_popups();
		}
		else if (frame == 15)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("OpenImage"));
		}
		else if (frame == 18)
		{
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 23)
		{
			CHECK(window_exists("Unsaved Changes"));
			CHECK(h.frontend.m_pending_file_action ==
			      PaintFrontend::FileAction::New);
		}
		else if (frame == 26)
		{
			close_all_popups();
			h.frontend.m_pending_file_action = PaintFrontend::FileAction::None;
		}
		else if (frame == 31)
		{
			CHECK(!app.is_modified());
			CHECK(ImGuiFileDialog::Instance()->IsOpened("SaveImage") == false);
		}
		else if (frame == 43)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("SaveImage"));
		}
		else if (frame == 46)
		{
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 50)
		{
			CHECK_FALSE(h.frontend.m_show_grid);
		}
		else if (frame == 56)
		{
			CHECK(h.frontend.m_show_grid);
		}
		else if (frame == 62)
		{
			CHECK(window_exists("Properties"));
		}
		else if (frame == 64)
		{
			close_all_popups();
		}
		else if (frame == 69)
		{
			CHECK(session.has_clipboard());
			CHECK(session.has_floating_region());
		}
		else if (frame == 74)
		{
			CHECK(session.has_clipboard());
			CHECK(session.has_floating_region());
		}
		else if (frame == 81)
		{
			CHECK(session.has_clipboard());
			CHECK_FALSE(session.has_floating_region());
		}

		SDL_Delay(frame_ms);
		++frame;
	}

	std::remove(path.c_str());

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
