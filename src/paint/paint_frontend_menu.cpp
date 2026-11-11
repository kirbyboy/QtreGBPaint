#include "paint/paint_frontend.h"
#include "paint_frontend_internal.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace PaintFrontendInternal;

void PaintFrontend::renderMenuBar()
{
	PaintSession& session = m_backend.session();

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N"))
			{
				request_new_document();
			}
			if (ImGui::MenuItem("Open", "Ctrl+O"))
			{
				request_open_document();
			}
			ImGui::Separator();
			const bool can_save =
				m_backend.is_modified() || !m_backend.has_file_path();
			if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save))
			{
				request_save();
			}
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
			{
				request_save_as();
			}
			ImGui::Separator();
			if (ImGui::BeginMenu("Export"))
			{
				if (ImGui::MenuItem("Bitmap..."))
				{
					request_export(ImageExportFormat::Bmp);
				}
				if (ImGui::MenuItem("PNG..."))
				{
					request_export(ImageExportFormat::Png);
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const std::vector<std::string>& recent = m_backend.recent_files();
			if (!recent.empty())
			{
				const std::size_t count =
					std::min<std::size_t>(recent.size(), 10);
				for (std::size_t index = 0; index < count; ++index)
				{
					const std::string& path = recent[index];
					const std::string name = recent_file_name(path);
					ImGui::PushID(static_cast<int>(index));
					if (ImGui::MenuItem(name.c_str()))
					{
						request_open_path(path);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", path.c_str());
					}
					ImGui::PopID();
				}
				ImGui::Separator();
			}
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				request_close_window();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_backend.can_undo()))
			{
				m_backend.undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_backend.can_redo()))
			{
				m_backend.redo();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A"))
			{
				select_all();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", "Ctrl+G", &m_show_grid);
			if (ImGui::MenuItem("Zoom +", "Ctrl++", false, m_zoom < 64))
			{
				request_zoom_steps(1);
			}
			if (ImGui::MenuItem("Zoom -", "Ctrl+-", false, m_zoom > 1))
			{
				request_zoom_steps(-1);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Selection"))
		{
			const bool has_floating = session.has_floating_region();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, has_floating))
			{
				copy_selection();
			}
			if (ImGui::MenuItem("Cut", "Ctrl+X", false, has_floating))
			{
				cut_selection();
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, session.has_clipboard()))
			{
				paste_selection();
			}
			if (ImGui::MenuItem("Delete", "Del", false, has_floating))
			{
				session.delete_floating_region();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Flip/Rotate...", "Ctrl+R", false,
			                    has_floating))
			{
				open_flip_rotate_dialog(FlipRotateTarget::Selection);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Invert Colors", nullptr, false, has_floating))
			{
				session.invert_floating_region_colors();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Image"))
		{
			if (ImGui::MenuItem("Flip/Rotate...", "Ctrl+R"))
			{
				open_flip_rotate_dialog(FlipRotateTarget::Canvas);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Properties...", "Ctrl+E"))
			{
				open_properties_dialog();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Palette"))
		{
			const auto palette_item = [this](const char* label, PaletteMode mode)
			{
				if (ImGui::MenuItem(label, nullptr,
				                    m_backend.palette_mode() == mode))
				{
					m_backend.select_palette(mode);
				}
			};

			palette_item("DMG", PaletteMode::Dmg);
			palette_item("Grayscale", PaletteMode::Grayscale);
			palette_item("Red", PaletteMode::Red);
			palette_item("Green", PaletteMode::Green);
			palette_item("Blue", PaletteMode::Blue);

			ImGui::Separator();
			if (ImGui::MenuItem("Invert Palette", nullptr,
			                    m_backend.palette_inverted()))
			{
				m_backend.toggle_palette_inverted();
			}

			ImGui::Separator();
			const std::optional<PaintCore::ColorIndex> transparent =
				m_backend.transparent_index();
			const PaintCore::ColorIndex foreground =
				m_backend.foreground_index();
			const bool foreground_is_transparent =
				transparent.has_value() && *transparent == foreground;
			const bool foreground_visible = is_visual_palette_index(
				foreground, m_backend.session().getBitsPerPixel());
			if (ImGui::MenuItem("Transparent Foreground", nullptr, false,
			                    foreground_visible &&
			                    !foreground_is_transparent))
			{
				m_backend.set_transparent_foreground();
			}
			if (ImGui::MenuItem("Clear Transparent Color", nullptr, false,
			                    m_backend.has_transparent_color()))
			{
				m_backend.clear_transparent_color();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			if (ImGui::BeginMenu("Theme"))
			{
				const auto theme_item = [this](const char* label, Theme value)
				{
					if (ImGui::MenuItem(label, nullptr, m_theme == value))
					{
						set_theme(value);
					}
				};

				theme_item("Light", Theme::Light);
				theme_item("Dark", Theme::Dark);
				theme_item("Classic", Theme::Classic);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About QtreGBPaint..."))
			{
				m_open_about_popup = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}
