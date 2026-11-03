#ifndef QTREGBPAINT_IMGUI_MANAGER_H
#define QTREGBPAINT_IMGUI_MANAGER_H

#include "imgui.h"

class IGuiBackend;

class ImGuiManager
{
	public:
		ImGuiManager() = default;
		~ImGuiManager() = default;
		ImGuiManager(const ImGuiManager&) = delete;
		ImGuiManager& operator=(const ImGuiManager&) = delete;

		void init();
		void shutdown();
		ImGuiContext* context() const { return m_context; }

		void begin_frame(IGuiBackend& backend);
		ImDrawData* end_frame();

	private:
		ImGuiContext* m_context = nullptr;
		bool m_initialized = false;
};

#endif
