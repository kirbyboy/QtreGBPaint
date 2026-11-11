#include "paint/paint_frontend.h"
#include "paint_frontend_internal.h"
#include "paint/i18n.h"

#include <cfloat>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "ImGuiFileDialog.h"

using namespace PaintFrontendInternal;

std::string PaintFrontend::file_dialog_directory() const
{
	if (!m_last_directory.empty() && accessible_directory(m_last_directory))
	{
		return m_last_directory;
	}

	// A stale or unreadable last directory must never produce an error. Fall
	// back silently to Pictures, then the home directory, then the cwd.
	if (const char* pictures = std::getenv("XDG_PICTURES_DIR"))
	{
		if (pictures[0] != '\0' && accessible_directory(pictures))
		{
			return pictures;
		}
	}

	if (const char* home = std::getenv("HOME"))
	{
		if (home[0] != '\0')
		{
			const std::filesystem::path home_path(home);
			const std::filesystem::path pictures_path = home_path / "Pictures";
			if (accessible_directory(pictures_path))
			{
				return pictures_path.string();
			}
			if (accessible_directory(home_path))
			{
				return home_path.string();
			}
		}
	}

	return ".";
}

void PaintFrontend::remember_file_directory(const std::string& path)
{
	const std::filesystem::path parent =
		std::filesystem::path(path).parent_path();
	if (!parent.empty())
	{
		m_last_directory = parent.string();
	}
}

void PaintFrontend::request_new_document()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::New;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	m_new_width = 256;
	m_new_height = 256;
	m_new_bpp = m_backend.session().getBitsPerPixel();
	m_open_new_popup = true;
}

void PaintFrontend::request_open_document()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::Open;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	open_open_dialog();
}

void PaintFrontend::request_open_path(std::string path)
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::OpenPath;
		m_pending_open_path = std::move(path);
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	m_pending_open_path.clear();
	open_recent_path(std::move(path));
}

void PaintFrontend::open_recent_path(std::string path)
{
	std::string error;
	FileError error_code = FileError::None;
	if (!m_backend.open_document(path, error, &error_code))
	{
		// Opening a recent entry is a single-shot action: report the error and
		// leave the current document untouched.
		show_file_error(error_code, error);
		return;
	}
	remember_file_directory(path);
}

void PaintFrontend::request_save()
{
	if (!m_backend.has_file_path())
	{
		request_save_as();
		return;
	}
	save_to_path(m_backend.file().path(), false);
}

void PaintFrontend::request_save_as()
{
	m_save_then_pending = false;
	open_save_as_dialog();
}

void PaintFrontend::request_export(ImageExportFormat format)
{
	const bool bitmap = format == ImageExportFormat::Bmp;
	const char* extension = bitmap ? ".bmp" : ".png";
	const char* dialog_key = bitmap ? "ExportBitmap" : "ExportPng";
	const std::string title =
		i18n::tr(bitmap ? "Export Bitmap" : "Export PNG");
	const std::string filters =
		std::string(i18n::tr(bitmap ? "Bitmap" : "PNG")) +
		(bitmap ? "{.bmp}" : "{.png}");

	std::string base_name = "untitled";
	std::string directory = file_dialog_directory();
	if (m_backend.has_file_path())
	{
		const std::filesystem::path document_path(m_backend.file().path());
		if (!document_path.stem().empty())
		{
			base_name = document_path.stem().string();
		}
		const std::filesystem::path parent = document_path.parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			directory = parent.string();
		}
	}

	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Default;
	config.path = directory;
	config.fileName = base_name + extension;
	ImGuiFileDialog::Instance()->OpenDialog(
		dialog_key, title.c_str(), filters.c_str(), config);
}

void PaintFrontend::export_to_path(const std::string& path,
                                   ImageExportFormat format)
{
	std::string error;
	FileError error_code = FileError::None;
	if (!m_backend.export_image(path, format, error, &error_code))
	{
		show_file_error(error_code, error);
		return;
	}
	remember_file_directory(path);
}

void PaintFrontend::request_close_window()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::Quit;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_should_close = true;
}

void PaintFrontend::perform_pending_file_action()
{
	const FileAction action = m_pending_file_action;
	const std::string open_path = std::move(m_pending_open_path);
	m_pending_file_action = FileAction::None;
	m_pending_open_path.clear();

	switch (action)
	{
		case FileAction::New:
			m_new_width = 256;
			m_new_height = 256;
			m_new_bpp = m_backend.session().getBitsPerPixel();
			m_open_new_popup = true;
			break;
		case FileAction::Open:
			open_open_dialog();
			break;
		case FileAction::OpenPath:
			open_recent_path(open_path);
			break;
		case FileAction::Quit:
			m_should_close = true;
			break;
		case FileAction::None:
			break;
	}
}

void PaintFrontend::open_open_dialog()
{
	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Modal |
	               ImGuiFileDialogFlags_HideColumnType;
	if (m_backend.has_file_path())
	{
		const std::filesystem::path parent = std::filesystem::path(
			m_backend.file().path()).parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			config.path = parent.string();
		}
		else
		{
			config.path = file_dialog_directory();
		}
	}
	else
	{
		config.path = file_dialog_directory();
	}
	const std::string filters =
		std::string(i18n::tr("Indexed Image")) + "{.idx}," +
		i18n::tr("All files") + "{.*}";
	// ImGuiFileDialog 0.6.8 has no runtime text API: its own labels are
	// compile-time macros, so only the title and filters are localized here.
	ImGuiFileDialog::Instance()->OpenDialog(
		"OpenImage", i18n::tr("Open Image"), filters.c_str(), config);
}

void PaintFrontend::open_save_as_dialog()
{
	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Default;
	if (m_backend.has_file_path())
	{
		const std::filesystem::path document_path(m_backend.file().path());
		const std::filesystem::path parent = document_path.parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			config.filePathName = m_backend.file().path();
		}
		else
		{
			config.path = file_dialog_directory();
			config.fileName = document_path.filename().string();
		}
	}
	else
	{
		config.path = file_dialog_directory();
		config.fileName = "untitled.idx";
	}
	ImGuiFileDialog::Instance()->OpenDialog(
		"SaveImage", i18n::tr("Save Image"), ".idx", config);
}

void PaintFrontend::save_to_path(const std::string& path, bool continue_pending)
{
	save_to_path(path, continue_pending, m_backend.file_format());
}

void PaintFrontend::save_to_path(const std::string& path, bool continue_pending,
                                 IndexedFormat format)
{
	std::string error;
	FileError error_code = FileError::None;
	if (!m_backend.save_document_as(path, format, error, &error_code))
	{
		m_save_then_pending = false;
		m_pending_file_action = FileAction::None;
		m_pending_open_path.clear();
		show_file_error(error_code, error);
		return;
	}

	remember_file_directory(path);

	if (continue_pending)
	{
		perform_pending_file_action();
	}
}

void PaintFrontend::show_file_error(FileError code, const std::string& message)
{
	m_error_code = code;
	m_error_message = message;
	if (m_error_message.empty() && code == FileError::None)
	{
		m_error_message = i18n::tr("Unknown error");
	}
	m_open_error_popup = true;
}
