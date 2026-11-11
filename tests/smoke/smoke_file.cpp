// Smoke test for the File menu UI state and ImGuiFileDialog integration.
// It does not click through the dialogs; it opens/closes them and checks the
// popups state so a broken integration fails early.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_file.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_file

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/indexed_image.h"

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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

void close_all_popups()
{
	ImGui::ClosePopupToLevel(0, true);
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

ImGuiWindow* find_window_prefix(const char* prefix)
{
	const std::size_t length = std::strlen(prefix);
	for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
	{
		if (window->Name && std::strncmp(window->Name, prefix, length) == 0)
		{
			return window;
		}
	}
	return nullptr;
}

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	Harness h;
	h.imgui.init();
	h.backend.init();

	PaintBackend& app = h.resources;
	PaintSession& canvas = app.session();
	std::string error;
	if (!app.new_document(4, 4, error))
	{
		std::cerr << "new_document failed: " << error << "\n";
		return 2;
	}

	if (!canvas.document().apply_pixel_patch(
			PaintCore::Rect{0, 0, 2, 2}, {1, 2, 3, 0}))
	{
		std::cerr << "apply_pixel_patch failed\n";
		return 2;
	}

	const std::string path = "temp/smoke_file.idx";
	std::remove(path.c_str());
	h.frontend.save_to_path(path, false);
	CHECK(app.has_file_path());
	CHECK_EQ(app.file().path(), path);
	CHECK(!app.is_modified());

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			if (!canvas.document().apply_pixel_patch(
					PaintCore::Rect{3, 3, 1, 1}, {1}))
			{
				std::cerr << "second patch failed\n";
				g_failures++;
			}
			CHECK(app.is_modified());
			h.frontend.request_save();
		}
		else if (frame == 4)
		{
			h.frontend.request_save_as();
		}
		else if (frame == 6)
		{
			h.frontend.request_open_document();
		}
		else if (frame == 8)
		{
			h.frontend.show_file_error("test error");
		}
		else if (frame == 10)
		{
			h.frontend.request_new_document();
		}
		else if (frame == 12)
		{
			if (!canvas.document().apply_pixel_patch(
					PaintCore::Rect{0, 3, 1, 1}, {2}))
			{
				std::cerr << "third patch failed\n";
				g_failures++;
			}
			CHECK(app.is_modified());
			h.frontend.request_new_document();
		}
		else if (frame == 13)
		{
			close_all_popups();
			h.frontend.m_pending_file_action = PaintFrontend::FileAction::None;
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 3)
		{
			CHECK(!app.is_modified());
		}
		else if (frame == 4)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("SaveImage"));
			ImGuiWindow* dialog = find_window_prefix("Save Image");
			CHECK(dialog != nullptr);
			if (dialog)
			{
				CHECK(dialog->Size.x >= 720.0f);
				CHECK(dialog->Size.y >= 480.0f);
			}
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 6)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("OpenImage"));
			ImGuiWindow* dialog = find_window_prefix("Open Image");
			CHECK(dialog != nullptr);
			if (dialog)
			{
				CHECK(dialog->Size.x >= 720.0f);
				CHECK(dialog->Size.y >= 480.0f);
			}
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 8)
		{
			CHECK(window_exists("File Error###file_error"));
			close_all_popups();
		}
		else if (frame == 10)
		{
			CHECK(window_exists("New Image###new_image"));
			close_all_popups();
		}
		else if (frame == 12)
		{
			CHECK(window_exists("Unsaved Changes###unsaved_changes"));
		}

		SDL_Delay(frame_ms);
		++frame;
	}

	const IndexedImage saved = IndexedImage::loadFromFile(path);
	CHECK(saved.isValid());
	CHECK_EQ(saved.width(), 4);
	CHECK_EQ(saved.height(), 4);

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
