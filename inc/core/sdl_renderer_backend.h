#ifndef QTREGBPAINT_SDL_BACKEND_H
#define QTREGBPAINT_SDL_BACKEND_H

#include <array>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>
#include "ibackend.h"
#include "core/window_geometry.h"


class SDLRendererBackend : public IGuiBackend
{
	public:
		SDLRendererBackend();

		void init() override;
		// Same as init(), but creates the window hidden, applies the stored
		// geometry and then shows it. Avoids a visible jump on startup.
		void init_with_geometry(const PaintCore::WindowGeometry& geometry);
		PaintCore::WindowGeometry window_geometry() const;

		void shutdown() override;

		void begin_imgui_frame() override;
		void render_imgui(ImDrawData* drawData) override;

		void set_minimum_window_size(int width, int height) override;
		void set_cursor(PaintCore::CursorType cursor) override;
		bool set_cursor_image(PaintCore::CursorType cursor,
		                      const PaintCore::CursorImage& image) override;
		void poll_events() override;
		bool quit_requested() const override { return m_quit_requested; }
		void clear_quit_request() override { m_quit_requested = false; }
		
		void* createTexture(int width, int height) override;
		void updateTexture(void* textureId, const PaintCore::Color* pixels, int width, int height) override;
		void updateTextureRegion(void* textureId, const PaintCore::Color* pixels,
		                         int x, int y, int width, int height) override;
		void destroyTexture(void* textureId) override;

	private:
		static constexpr std::size_t kToolCursorCount =
			static_cast<std::size_t>(PaintCore::CursorType::Count);

		SDL_Window* m_window = nullptr;
		SDL_Renderer* m_renderer = nullptr;
		std::unordered_map<void*, SDL_Texture*> m_textures;
		bool m_imgui_ready = false;
		bool m_quit_requested = false;
		int m_minimum_width = 0;
		int m_minimum_height = 0;
		PaintCore::WindowGeometry m_normal_geometry;
		bool m_window_maximized = false;

		PaintCore::CursorType m_requested_cursor = PaintCore::CursorType::Arrow;
		std::array<SDL_Cursor*, kToolCursorCount> m_tool_cursors{};
		std::vector<SDL_Cursor*> m_imgui_cursors;
		SDL_Cursor* m_last_cursor = nullptr;

		void create_cursors();
		void destroy_cursors();
		void update_cursor();
		void update_normal_geometry();
		SDL_Cursor* tool_cursor(PaintCore::CursorType cursor) const;
};

#endif
