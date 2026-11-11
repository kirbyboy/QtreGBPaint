// Smoke test: clicking a recent file opens it directly, and a failed open only
// shows the File Error modal without touching the current document.

#include <SDL.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include "imgui.h"

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

#define CHECK_FALSE(expr) CHECK(!(expr))

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	const std::filesystem::path root = "/tmp/opencode/smoke_recent";
	std::error_code filesystem_error;
	std::filesystem::remove_all(root, filesystem_error);
	std::filesystem::create_directories(root);

	const std::string first = (root / "first.idx").string();
	const std::string second = (root / "second.idx").string();

	Harness h;
	h.imgui.init();
	h.backend.init();

	PaintBackend& app = h.resources;
	std::string error;
	REQUIRE(app.new_document(4, 4, error));
	REQUIRE(app.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 1, 1}, {1}));
	REQUIRE(app.save_document_as(first, error));
	REQUIRE(app.save_document_as(second, error));
	CHECK(app.file().path() == second);

	// Recent entry with no unsaved changes: direct open, no dialog.
	h.frontend.request_open_path(first);
	CHECK(app.file().path() == first);
	CHECK_FALSE(h.frontend.m_open_error_popup);
	CHECK(h.frontend.m_pending_open_path.empty());

	// Missing file: only the error modal is shown; the document stays as-is.
	h.frontend.request_open_path((root / "missing.idx").string());
	CHECK(h.frontend.m_open_error_popup);
	CHECK_FALSE(h.frontend.m_error_message.empty());
	CHECK(app.file().path() == first);
	CHECK_FALSE(h.frontend.m_pending_file_action ==
	            PaintFrontend::FileAction::OpenPath);

	// Unsaved changes: the pending recent path survives the Unsaved Changes
	// flow and is opened when the action is resumed (Discard or after Save).
	h.frontend.m_open_error_popup = false;
	REQUIRE(app.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 1, 1}, {2}));
	CHECK(app.is_modified());
	h.frontend.request_open_path(second);
	CHECK(h.frontend.m_pending_file_action ==
	      PaintFrontend::FileAction::OpenPath);
	CHECK(h.frontend.m_open_unsaved_popup);
	CHECK(h.frontend.m_pending_open_path == second);

	h.frontend.perform_pending_file_action();
	CHECK(app.file().path() == second);
	CHECK_FALSE(app.is_modified());
	CHECK(h.frontend.m_pending_open_path.empty());

	h.frontend.m_open_error_popup = false;
	h.frontend.m_open_unsaved_popup = false;

	h.backend.shutdown();
	h.imgui.shutdown();

	std::filesystem::remove_all(root, filesystem_error);
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
