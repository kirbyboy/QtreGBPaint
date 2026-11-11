#include "paint/paint_frontend.h"
#include "paint_frontend_internal.h"
#include "paint/png_image.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

#include "ImGuiFileDialog.h"

using namespace PaintFrontendInternal;

void PaintFrontend::ensure_about_logo()
{
	if (m_about_logo_attempted || m_about_logo_path.empty())
	{
		return;
	}

	m_about_logo_attempted = true;
	PaintCore::RgbaImage image;
	std::string error;
	if (!PaintCore::load_png_rgba(m_about_logo_path, image, error))
	{
		std::fprintf(stderr, "About logo: %s\n", error.c_str());
		return;
	}

	IGuiBackend* backend = m_backend.backend();
	if (!backend)
	{
		return;
	}

	void* texture = backend->createTexture(image.width, image.height);
	if (!texture)
	{
		std::fprintf(stderr, "About logo: could not create the texture\n");
		return;
	}

	backend->updateTexture(texture, image.pixels.data(), image.width,
	                       image.height);
	m_about_logo_texture = texture;
	m_about_logo_width = image.width;
	m_about_logo_height = image.height;
}

void PaintFrontend::renderDialogs()
{
	if (m_open_about_popup)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
			       viewport->Pos.y + viewport->Size.y * 0.5f),
			ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::OpenPopup("About QtreGBPaint");
		m_open_about_popup = false;
	}

	if (ImGui::BeginPopupModal("About QtreGBPaint", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ensure_about_logo();
		if (m_about_logo_texture)
		{
			const float max_logo = 160.0f;
			const float largest = static_cast<float>(
				std::max(m_about_logo_width, m_about_logo_height));
			const float scale = std::min(1.0f, max_logo / largest);
			const ImVec2 logo_size(
				static_cast<float>(m_about_logo_width) * scale,
				static_cast<float>(m_about_logo_height) * scale);
			const float available = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(
				ImGui::GetCursorPosX() +
				std::max(0.0f, (available - logo_size.x) * 0.5f));
			ImGui::Image(
				reinterpret_cast<ImTextureID>(m_about_logo_texture),
				logo_size);
			ImGui::Spacing();
		}

		const auto centered_text = [](const char* text)
		{
			const float text_width = ImGui::CalcTextSize(text).x;
			const float available = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(
				ImGui::GetCursorPosX() +
				std::max(0.0f, (available - text_width) * 0.5f));
			ImGui::TextUnformatted(text);
		};

		centered_text("QtreGBPaint");
		centered_text("Made by kirbyboy");
		ImGui::Separator();
		ImGui::TextUnformatted("Copyright (C) 2026 the QtreGBPaint project");
		ImGui::TextUnformatted(
			"License: GNU General Public License v3.0 or later");
		ImGui::TextDisabled("Code: GPL-3.0-or-later");
		ImGui::TextDisabled("Visual assets: CC0 1.0");
		ImGui::TextDisabled("https://www.gnu.org/licenses/gpl-3.0.html");
		ImGui::TextDisabled("https://github.com/kirbyboy/QtreGBPaint");
		ImGui::Spacing();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_open_properties_popup)
	{
		ImGui::OpenPopup("Properties");
		m_open_properties_popup = false;
	}

	if (ImGui::BeginPopupModal("Properties", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Width", &m_canvas_size_width);
		ImGui::InputInt("Height", &m_canvas_size_height);
		m_canvas_size_width = std::max(1, m_canvas_size_width);
		m_canvas_size_height = std::max(1, m_canvas_size_height);
		ImGui::SliderInt("Bits per pixel", &m_canvas_size_bpp, 1, 8);
		m_canvas_size_bpp = std::clamp(m_canvas_size_bpp, 1, 8);
		ImGui::Text("Colors: %d", palette_usable_color_count(m_canvas_size_bpp));
		ImGui::TextDisabled("New area uses the current background color.");
		if (m_canvas_size_bpp != m_backend.session().getBitsPerPixel())
		{
			ImGui::Spacing();
			ImGui::TextUnformatted(
				"Changing bits per pixel rewrites the image:");
			ImGui::TextUnformatted(
				"all pixels are remapped to the closest tone");
			ImGui::TextUnformatted(
				"in the new palette; the saved file uses the new format.");
		}

		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			const int target_bpp = m_canvas_size_bpp;
			m_backend.resize_canvas(m_canvas_size_width, m_canvas_size_height);
			if (target_bpp != m_backend.session().getBitsPerPixel())
			{
				m_backend.change_bits_per_pixel(target_bpp);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_open_flip_rotate_popup)
	{
		ImGui::OpenPopup("Flip/Rotate");
		m_open_flip_rotate_popup = false;
	}

	if (ImGui::BeginPopupModal("Flip/Rotate", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(
			m_flip_rotate_target == FlipRotateTarget::Selection
				? "Transform selection"
				: "Transform image");
		ImGui::RadioButton("Flip horizontal", &m_flip_rotate_mode, 0);
		ImGui::RadioButton("Flip vertical", &m_flip_rotate_mode, 1);
		ImGui::RadioButton("Rotate", &m_flip_rotate_mode, 2);
		if (m_flip_rotate_mode == 2)
		{
			ImGui::Indent();
			ImGui::RadioButton("90 degrees", &m_flip_rotate_angle, 90);
			ImGui::SameLine();
			ImGui::RadioButton("180 degrees", &m_flip_rotate_angle, 180);
			ImGui::SameLine();
			ImGui::RadioButton("270 degrees", &m_flip_rotate_angle, 270);
			ImGui::Unindent();
		}
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			apply_flip_rotate();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Global shortcuts. Shortcut respects focus and text widgets, and does not
	// consume the key when there is no history available. Skip them while a
	// popup (menu, file dialog or modal) owns the input.
	const bool popup_open = ImGui::IsPopupOpen(
		"", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (!popup_open)
	{
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) && m_backend.can_undo())
		{
			m_backend.undo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) && m_backend.can_redo())
		{
			m_backend.redo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
		{
			request_new_document();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
		{
			request_open_document();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal) &&
		    (m_backend.is_modified() || !m_backend.has_file_path()))
		{
			request_save();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
		                    ImGuiInputFlags_RouteGlobal))
		{
			request_save_as();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_G,
		                    ImGuiInputFlags_RouteGlobal))
		{
			m_show_grid = !m_show_grid;
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A,
		                    ImGuiInputFlags_RouteGlobal))
		{
			select_all();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_R,
		                    ImGuiInputFlags_RouteGlobal))
		{
			// With a floating selection Ctrl+R transforms it; otherwise it
			// transforms the whole image. The Image menu always targets the
			// canvas.
			open_flip_rotate_dialog(
				m_backend.session().has_floating_region()
					? FlipRotateTarget::Selection
					: FlipRotateTarget::Canvas);
		}
		const bool zoom_in =
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd,
			                ImGuiInputFlags_RouteGlobal);
		if (zoom_in)
		{
			request_zoom_steps(1);
		}
		const bool zoom_out =
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract,
			                ImGuiInputFlags_RouteGlobal);
		if (zoom_out)
		{
			request_zoom_steps(-1);
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E,
		                    ImGuiInputFlags_RouteGlobal))
		{
			open_properties_dialog();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_floating_region())
		{
			copy_selection();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_floating_region())
		{
			cut_selection();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_clipboard())
		{
			paste_selection();
		}
		if (m_backend.session().has_floating_region() &&
		    !ImGui::GetIO().WantTextInput &&
		    ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			m_backend.session().delete_floating_region();
		}
		if (m_backend.session().has_pending_edit())
		{
			if (ImGui::Shortcut(ImGuiKey_Enter, ImGuiInputFlags_RouteGlobal))
			{
				m_backend.session().commit_pending_edit();
			}
			if (ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteGlobal))
			{
				m_backend.session().cancel_pending_edit();
			}
		}
	}
}

void PaintFrontend::renderFileDialogs()
{
	if (m_open_new_popup)
	{
		m_open_new_popup = false;
		ImGui::OpenPopup("New Image");
	}
	if (m_open_unsaved_popup)
	{
		m_open_unsaved_popup = false;
		ImGui::OpenPopup("Unsaved Changes");
	}
	if (m_open_error_popup)
	{
		m_open_error_popup = false;
		ImGui::OpenPopup("File Error");
	}

	if (ImGuiFileDialog::Instance()->Display("OpenImage",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string error;
			const std::string path =
				ImGuiFileDialog::Instance()->GetFilePathName();
			if (!m_backend.open_document(path, error))
			{
				show_file_error(error);
			}
			else
			{
				remember_file_directory(path);
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("SaveImage",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			m_pending_save_path = ensure_idx_extension(
				ImGuiFileDialog::Instance()->GetFilePathName());
			m_save_format = m_backend.file_format();
			m_open_idx_options_popup = true;
		}
		else
		{
			m_save_then_pending = false;
			if (m_pending_file_action != FileAction::None)
			{
				m_pending_file_action = FileAction::None;
			}
			m_pending_open_path.clear();
			m_pending_save_path.clear();
			m_open_idx_options_popup = false;
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("ExportBitmap",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			const std::string path = ensure_extension(
				ImGuiFileDialog::Instance()->GetFilePathName(), ".bmp");
			export_to_path(path, ImageExportFormat::Bmp);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("ExportPng",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			const std::string path = ensure_extension(
				ImGuiFileDialog::Instance()->GetFilePathName(), ".png");
			export_to_path(path, ImageExportFormat::Png);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (m_open_idx_options_popup)
	{
		m_open_idx_options_popup = false;
		if (!m_pending_save_path.empty())
		{
			ImGui::OpenPopup("IDX Options");
		}
	}

	if (ImGui::BeginPopupModal("IDX Options", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Storage format for the .idx file:");
		ImGui::TextUnformatted("Format");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0f);
		static const char* formats[] = {
			"Index", "Binary", "Packed", "BinaryPacked"};
		int format_index = static_cast<int>(m_save_format);
		if (ImGui::Combo("##idx_format", &format_index, formats, 4))
		{
			m_save_format = static_cast<IndexedFormat>(format_index);
		}

		ImGui::BeginChild("##idx_format_help", ImVec2(470.0f, 64.0f), true);
		ImGui::TextWrapped("%s", indexed_format_description(m_save_format));
		ImGui::EndChild();

		const int width = m_backend.session().getWidth();
		const int height = m_backend.session().getHeight();
		const int bpp = m_backend.session().getBitsPerPixel();
		ImGui::TextUnformatted("Approx. file sizes:");
		if (ImGui::BeginTable("##idx_sizes", 2,
		                      ImGuiTableFlags_SizingFixedFit |
		                          ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Format");
			ImGui::TableSetupColumn("Approx. size");
			ImGui::TableHeadersRow();
			for (IndexedFormat format : {IndexedFormat::Index,
			                             IndexedFormat::Binary,
			                             IndexedFormat::Packed,
			                             IndexedFormat::BinaryPacked})
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(indexed_format_name(format));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(format_byte_size(
					indexed_image_estimated_file_size(
						width, height, bpp, format)).c_str());
			}
			ImGui::EndTable();
		}
		const std::size_t data_size = indexed_image_data_size(
			width, height, bpp, m_save_format);
		ImGui::TextDisabled("Selected data: %s + ~64 B header",
		                    format_byte_size(data_size).c_str());

		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(80.0f, 0.0f)))
		{
			const std::string path = std::move(m_pending_save_path);
			m_pending_save_path.clear();
			const bool continue_pending = m_save_then_pending;
			m_save_then_pending = false;
			ImGui::CloseCurrentPopup();
			save_to_path(path, continue_pending, m_save_format);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			m_save_then_pending = false;
			m_pending_file_action = FileAction::None;
			m_pending_open_path.clear();
			m_pending_save_path.clear();
		}
		ImGui::EndPopup();
	}

	// If the modal was dismissed without pressing Save/Cancel, discard the
	// pending path so the next Save/Save As starts cleanly.
	if (!m_open_idx_options_popup && !m_pending_save_path.empty() &&
	    !ImGui::IsPopupOpen("IDX Options"))
	{
		m_pending_save_path.clear();
		m_save_then_pending = false;
		m_pending_file_action = FileAction::None;
		m_pending_open_path.clear();
	}

	if (ImGui::BeginPopupModal("New Image", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Width", &m_new_width);
		ImGui::InputInt("Height", &m_new_height);
		m_new_width = std::max(1, m_new_width);
		m_new_height = std::max(1, m_new_height);
		ImGui::SliderInt("Bits per pixel", &m_new_bpp, 1, 8);
		m_new_bpp = std::clamp(m_new_bpp, 1, 8);
		ImGui::Text("Colors: %d", palette_usable_color_count(m_new_bpp));
		ImGui::Separator();

		if (ImGui::Button("Create", ImVec2(80.0f, 0.0f)))
		{
			std::string error;
			if (m_backend.new_document(m_new_width, m_new_height,
			                           m_new_bpp, error))
			{
				ImGui::CloseCurrentPopup();
			}
			else
			{
				ImGui::CloseCurrentPopup();
				show_file_error(error);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		const std::string name = m_backend.file_display_name();
		const char* question = m_pending_file_action == FileAction::Quit
			? "before closing?"
			: "before continuing?";
		ImGui::Text("Save changes to \"%s\" %s", name.c_str(), question);
		ImGui::Separator();

		if (ImGui::Button("Save", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			if (!m_backend.has_file_path())
			{
				m_save_then_pending = true;
				open_save_as_dialog();
			}
			else
			{
				std::string error;
				if (m_backend.save_document(error))
				{
					perform_pending_file_action();
				}
				else
				{
					m_pending_file_action = FileAction::None;
					m_pending_open_path.clear();
					show_file_error(error);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			perform_pending_file_action();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			m_pending_file_action = FileAction::None;
			m_pending_open_path.clear();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("File Error", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", m_error_message.c_str());
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
