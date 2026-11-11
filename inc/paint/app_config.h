#ifndef QTREGBPAINT_APP_CONFIG_H
#define QTREGBPAINT_APP_CONFIG_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/window_geometry.h"
#include "paint/theme.h"

// Small, human-readable application configuration stored outside the
// installation directory. The default path follows the XDG Base Directory
// specification:
//
//   $XDG_CONFIG_HOME/qtregbpaint/config.ini
//   ~/.config/qtregbpaint/config.ini
//
// That works for both a traditional package and an AppImage: the executable
// and its mount point may be read-only or ephemeral, while the config lives in
// the user's home. The format is a flat key=value INI-like file; paths are
// quoted with std::quoted so spaces and '=' are safe.
class AppConfig
{
public:
	static constexpr std::size_t kMaxRecentFiles = 10;

	// Missing or unreadable files produce the defaults. Unknown keys, malformed
	// values and comments are ignored.
	static AppConfig load(const std::string& path);
	static AppConfig load_default();
	static std::string default_path();

	// Writes atomically through a temporary file in the same directory.
	bool save(const std::string& path, std::string& error) const;
	bool save_default(std::string& error) const;

	AppTheme theme = AppTheme::Light;
	// "system", "en" or "es". The catalog itself lives in lang/<locale>.po.
	std::string language = "system";
	bool grid_enabled = true;
	PaintCore::WindowGeometry window;
	// Last directory used by Open/Save As. If it no longer exists or cannot be
	// read, the frontend silently falls back to Pictures or the home directory.
	std::string last_directory;
	std::vector<std::string> recent_files;
};

#endif
