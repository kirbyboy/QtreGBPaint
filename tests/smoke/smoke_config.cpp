// Smoke test: AppConfig persistence, ImGui ini disabled and SDL window
// geometry restore.
//
// Build/run through tests/smoke/run.sh config (it needs a display).

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/app_config.h"

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

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	const std::filesystem::path root = "/tmp/opencode/smoke_config";
	std::error_code error;
	std::filesystem::remove_all(root, error);
	std::filesystem::create_directories(root / "config");
	std::filesystem::create_directories(root / "work");

	const std::string config_home = (root / "config").string();
	setenv("XDG_CONFIG_HOME", config_home.c_str(), 1);
	std::filesystem::current_path(root / "work");

	CHECK_EQ(AppConfig::default_path(),
	         (root / "config" / "qtregbpaint" / "config.ini").string());

	AppConfig config;
	config.theme = AppTheme::Classic;
	config.grid_enabled = false;
	config.window = PaintCore::WindowGeometry{140, 90, 640, 480, false, true};
	config.last_directory = "/tmp/a folder";
	config.recent_files = {"/tmp/some file.idx", "/tmp/other.idx"};

	std::string save_error;
	REQUIRE(config.save_default(save_error));
	CHECK(save_error.empty());
	CHECK(std::filesystem::exists(
		root / "config" / "qtregbpaint" / "config.ini"));

	const AppConfig loaded = AppConfig::load_default();
	CHECK(loaded.theme == AppTheme::Classic);
	CHECK_FALSE(loaded.grid_enabled);
	CHECK_EQ(loaded.window.x, 140);
	CHECK_EQ(loaded.window.y, 90);
	CHECK_EQ(loaded.window.width, 640);
	CHECK_EQ(loaded.window.height, 480);
	CHECK(loaded.window.has_position);
	CHECK_EQ(loaded.last_directory, "/tmp/a folder");
	REQUIRE(loaded.recent_files.size() == 2u);
	CHECK_EQ(loaded.recent_files[0], "/tmp/some file.idx");

	ImGuiManager imgui;
	imgui.init();

	SDLRendererBackend backend;
	backend.init_with_geometry(loaded.window);

	CHECK_EQ(backend.window_geometry().width, 640);
	CHECK_EQ(backend.window_geometry().height, 480);
	CHECK(backend.window_geometry().has_position);

	// Let SDL process the show/geometry events before checking the flags.
	backend.poll_events();

	// ImGui must not read or write imgui.ini in the current working directory.
	CHECK_FALSE(std::filesystem::exists(
		std::filesystem::current_path() / "imgui.ini"));

	backend.shutdown();
	imgui.shutdown();

	CHECK_FALSE(std::filesystem::exists(
		std::filesystem::current_path() / "imgui.ini"));

	std::filesystem::remove_all(root, error);
	if (g_failures == 0)
	{
		std::cout << "SMOKE OK\n";
		return 0;
	}
	std::cerr << "SMOKE FAIL: " << g_failures << " failures\n";
	return 1;
}
}
// namespace

int main()
{
	return run();
}
