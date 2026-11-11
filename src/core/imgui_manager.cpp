#include "core/imgui_manager.h"
#include "core/ibackend.h"

void ImGuiManager::init()
{
	if(m_initialized)
	{
		return;
	}
	IMGUI_CHECKVERSION();
	m_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_context);

	ImGuiIO& io = ImGui::GetIO();

	// The application has a fixed layout and persists its own settings in
	// AppConfig. Do not read or write imgui.ini: the current working
	// directory is unreliable for installed packages and AppImages.
	io.IniFilename = nullptr;

	// Enable docking and other settings.
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // enable docking

	m_initialized = true;
}

void ImGuiManager::shutdown()
{
	if(!m_initialized)
	{
		return;
	}
	ImGui::DestroyContext(m_context);
	m_context = nullptr;
	m_initialized = false;
}

void ImGuiManager::begin_frame(IGuiBackend& backend)
{
	backend.begin_imgui_frame();
	ImGui::NewFrame();
}

ImDrawData* ImGuiManager::end_frame()
{
	ImGui::Render();
	return ImGui::GetDrawData();
}
