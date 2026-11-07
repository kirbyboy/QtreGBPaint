#ifndef QTREGBPAINT_SDL_BACKEND_H
#define QTREGBPAINT_SDL_BACKEND_H

#include <array>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>
#include "ibackend.h"


class SDLRendererBackend : public IGuiBackend
{
	public:
		SDLRendererBackend();

		void init() override;
		void shutdown() override;

		void begin_imgui_frame() override;
		void render_imgui(ImDrawData* drawData) override;

		void set_minimum_window_size(int width, int height) override;
		void set_cursor(PaintCore::CursorType cursor) override;
		bool set_cursor_image(PaintCore::CursorType cursor,
		                      const PaintCore::CursorImage& image) override;
		void poll_events(bool& running) override;
		
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
		int m_minimum_width = 0;
		int m_minimum_height = 0;

		PaintCore::CursorType m_requested_cursor = PaintCore::CursorType::Arrow;
		std::array<SDL_Cursor*, kToolCursorCount> m_tool_cursors{};
		std::vector<SDL_Cursor*> m_imgui_cursors;
		SDL_Cursor* m_last_cursor = nullptr;

		void create_cursors();
		void destroy_cursors();
		void update_cursor();
		SDL_Cursor* tool_cursor(PaintCore::CursorType cursor) const;
};

#endif
