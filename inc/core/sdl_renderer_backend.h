#ifndef QTREGBPAINT_SDL_BACKEND_H
#define QTREGBPAINT_SDL_BACKEND_H

#include <unordered_map>
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

		void poll_events(bool& running) override;
		
		void* createTexture(int width, int height) override;
		void updateTexture(void* textureId, const PaintCore::Color* pixels, int width, int height) override;
		void updateTextureRegion(void* textureId, const PaintCore::Color* pixels,
		                         int x, int y, int width, int height) override;
		void destroyTexture(void* textureId) override;

	private:
		SDL_Window* m_window = nullptr;
		SDL_Renderer* m_renderer = nullptr;
		std::unordered_map<void*, SDL_Texture*> m_textures;
		bool m_imgui_ready = false;
};

#endif
