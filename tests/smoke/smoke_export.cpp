// Smoke test: Export > Bitmap/PNG dialogs and the frontend export path.

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/rect.h"
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

std::vector<std::uint8_t> read_file(const std::string& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>());
}

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	const std::filesystem::path root = "/tmp/opencode/smoke_export";
	std::error_code filesystem_error;
	std::filesystem::remove_all(root, filesystem_error);
	std::filesystem::create_directories(root);

	Harness h;
	h.imgui.init();
	h.backend.init();

	PaintBackend& app = h.resources;
	std::string error;
	REQUIRE(app.new_document(2, 2, 2, error));
	REQUIRE(app.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 2, 2}, {0, 1, 2, 3}));

	bool running = true;
	int frame = 0;
	const std::string bmp_path = (root / "out.bmp").string();
	const std::string png_path = (root / "out.png").string();

	while (running)
	{
		if (frame == 1)
		{
			h.frontend.request_export(ImageExportFormat::Bmp);
		}
		else if (frame == 2)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("ExportBitmap"));
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 3)
		{
			h.frontend.request_export(ImageExportFormat::Png);
		}
		else if (frame == 4)
		{
			CHECK(ImGuiFileDialog::Instance()->IsOpened("ExportPng"));
			ImGuiFileDialog::Instance()->Close();
		}
		else if (frame == 5)
		{
			h.frontend.export_to_path(bmp_path, ImageExportFormat::Bmp);
			h.frontend.export_to_path(png_path, ImageExportFormat::Png);
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		SDL_Delay(1000 / 60);
		++frame;
	}

	const std::vector<std::uint8_t> bmp = read_file(bmp_path);
	const std::vector<std::uint8_t> png = read_file(png_path);
	REQUIRE(bmp.size() >= 54);
	CHECK(bmp[0] == 'B');
	CHECK(bmp[1] == 'M');
	REQUIRE(png.size() >= 8);
	static const std::uint8_t signature[8] = {
		0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	CHECK(std::equal(std::begin(signature), std::end(signature), png.begin()));
	CHECK_FALSE(h.frontend.m_open_error_popup);

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
