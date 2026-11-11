// Smoke test: vendored ImGuiFileDialog runtime strings switch with the UI
// language while the Open dialog is open.

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

#include "ImGuiFileDialog.h"
#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/i18n.h"
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

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

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
	i18n::set_language_directory(asset_base + "/../lang");

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Pencil", std::make_shared<PencilTool>(1, 2),
	                      PaintCore::CursorType::Pencil,
	                      asset_base + "/pencil_button.idx", gray);
	h.resources.selectButton("Pencil");

	h.frontend.set_language("es");
	CHECK_EQ(i18n::resolved_locale(), "es");
	CHECK_EQ(IGFD::FileDialog::GetStrings().okButton, "Aceptar");
	CHECK_EQ(IGFD::FileDialog::GetStrings().cancelButton, "Cancelar");
	CHECK_EQ(IGFD::FileDialog::GetStrings().search, "Buscar:");

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

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			h.frontend.open_open_dialog();
		}
		else if (frame == 4)
		{
			CHECK(IGFD::FileDialog::Instance()->IsOpened("OpenImage"));
			CHECK_EQ(IGFD::FileDialog::GetStrings().okButton, "Aceptar");
			save_frame(renderer, "temp/smoke_file_dialog_i18n_es.ppm");
		}
		else if (frame == 5)
		{
			h.frontend.set_language("en");
			CHECK_EQ(i18n::resolved_locale(), "en");
			CHECK_EQ(IGFD::FileDialog::GetStrings().okButton, "OK");
			CHECK_EQ(IGFD::FileDialog::GetStrings().cancelButton, "Cancel");
			CHECK_EQ(IGFD::FileDialog::GetStrings().search, "Search :");
		}
		else if (frame == 6)
		{
			// Reopen so the title and filters, which are fixed by OpenDialog(),
			// also reflect the new language in the screenshot.
			IGFD::FileDialog::Instance()->Close();
			h.frontend.open_open_dialog();
		}
		else if (frame == 8)
		{
			CHECK(IGFD::FileDialog::Instance()->IsOpened("OpenImage"));
			CHECK_EQ(IGFD::FileDialog::GetStrings().okButton, "OK");
			save_frame(renderer, "temp/smoke_file_dialog_i18n_en.ppm");
			IGFD::FileDialog::Instance()->Close();
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
