#include "paint/app_config.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
std::string trim(std::string_view view)
{
	const std::size_t first = view.find_first_not_of(" \t\r\n");
	if (first == std::string_view::npos)
	{
		return {};
	}
	const std::size_t last = view.find_last_not_of(" \t\r\n");
	return std::string(view.substr(first, last - first + 1));
}

std::string lowercase(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
	               [](unsigned char c)
	               {
		               return static_cast<char>(std::tolower(c));
	               });
	return value;
}

bool parse_int(const std::string& text, int& value)
{
	const char* begin = text.data();
	const char* end = begin + text.size();
	const auto result = std::from_chars(begin, end, value);
	return result.ec == std::errc() && result.ptr == end;
}

bool parse_bool(const std::string& text, bool& value)
{
	const std::string normalized = lowercase(trim(text));
	if (normalized == "1" || normalized == "true" ||
	    normalized == "yes" || normalized == "on")
	{
		value = true;
		return true;
	}
	if (normalized == "0" || normalized == "false" ||
	    normalized == "no" || normalized == "off")
	{
		value = false;
		return true;
	}
	return false;
}

const char* theme_name(AppTheme theme)
{
	switch (theme)
	{
		case AppTheme::Dark:
			return "dark";
		case AppTheme::Light:
			return "light";
		case AppTheme::Classic:
			return "classic";
	}
	return "light";
}

bool parse_theme(const std::string& text, AppTheme& theme)
{
	const std::string normalized = lowercase(trim(text));
	if (normalized == "dark")
	{
		theme = AppTheme::Dark;
		return true;
	}
	if (normalized == "light")
	{
		theme = AppTheme::Light;
		return true;
	}
	if (normalized == "classic")
	{
		theme = AppTheme::Classic;
		return true;
	}
	return false;
}

bool valid_path_text(const std::string& path)
{
	for (unsigned char character : path)
	{
		// Control bytes and NULs indicate a corrupted configuration entry.
		// UTF-8 bytes are >= 0x80 and remain valid.
		if (character < 0x20 || character == 0x7f)
		{
			return false;
		}
	}
	return true;
}

void add_recent_file(std::vector<std::string>& files, std::string path)
{
	if (path.empty() || !valid_path_text(path))
	{
		return;
	}
	if (std::find(files.begin(), files.end(), path) != files.end())
	{
		return;
	}
	if (files.size() >= AppConfig::kMaxRecentFiles)
	{
		return;
	}
	files.push_back(std::move(path));
}

bool parse_path(const std::string& value, std::string& path)
{
	const std::size_t first = value.find_first_not_of(" \t");
	if (first != std::string::npos && value[first] == '"')
	{
		std::istringstream stream(value.substr(first));
		if (!(stream >> std::quoted(path)))
		{
			return false;
		}
		std::string trailing;
		stream >> trailing;
		return trailing.empty() && valid_path_text(path) && !path.empty();
	}

	// Accept hand-written unquoted values too, including leading spaces in
	// the path. We only wrote quoted values ourselves.
	path = value;
	return valid_path_text(path) && !path.empty();
}
}

AppConfig AppConfig::load(const std::string& path)
{
	AppConfig config;
	if (path.empty())
	{
		return config;
	}

	std::ifstream input(path);
	if (!input)
	{
		return config;
	}

	bool has_x = false;
	bool has_y = false;
	std::string line;
	while (std::getline(input, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		const std::string normalized = trim(line);
		if (normalized.empty() || normalized.front() == '#' ||
		    normalized.front() == ';')
		{
			continue;
		}

		const std::size_t separator = normalized.find('=');
		if (separator == std::string::npos)
		{
			continue;
		}

		const std::string key = trim(normalized.substr(0, separator));
		const std::string value = normalized.substr(separator + 1);
		int number = 0;

		if (key == "theme")
		{
			AppTheme theme = AppTheme::Light;
			if (parse_theme(value, theme))
			{
				config.theme = theme;
			}
		}
		else if (key == "grid")
		{
			bool enabled = true;
			if (parse_bool(value, enabled))
			{
				config.grid_enabled = enabled;
			}
		}
		else if (key == "window.x")
		{
			if (parse_int(trim(value), number))
			{
				config.window.x = number;
				has_x = true;
			}
		}
		else if (key == "window.y")
		{
			if (parse_int(trim(value), number))
			{
				config.window.y = number;
				has_y = true;
			}
		}
		else if (key == "window.width")
		{
			if (parse_int(trim(value), number) && number > 0)
			{
				config.window.width = number;
			}
		}
		else if (key == "window.height")
		{
			if (parse_int(trim(value), number) && number > 0)
			{
				config.window.height = number;
			}
		}
		else if (key == "window.maximized")
		{
			bool maximized = false;
			if (parse_bool(value, maximized))
			{
				config.window.maximized = maximized;
			}
		}
		else if (key == "last_directory")
		{
			std::string directory;
			if (parse_path(value, directory))
			{
				config.last_directory = std::move(directory);
			}
		}
		else if (key == "recent_file")
		{
			std::string recent;
			if (parse_path(value, recent))
			{
				add_recent_file(config.recent_files, std::move(recent));
			}
		}
	}

	config.window.has_position = has_x && has_y;
	return config;
}

AppConfig AppConfig::load_default()
{
	return load(default_path());
}

std::string AppConfig::default_path()
{
	if (const char* config_home = std::getenv("XDG_CONFIG_HOME"))
	{
		if (*config_home != '\0')
		{
			return (std::filesystem::path(config_home) / "qtregbpaint" /
			        "config.ini").string();
		}
	}

	if (const char* home = std::getenv("HOME"))
	{
		if (*home != '\0')
		{
			return (std::filesystem::path(home) / ".config" / "qtregbpaint" /
			        "config.ini").string();
		}
	}

	return {};
}

bool AppConfig::save(const std::string& path, std::string& error) const
{
	error.clear();
	if (path.empty())
	{
		error = "No configuration path (HOME is not set)";
		return false;
	}

	const std::filesystem::path file_path(path);
	std::error_code io_error;
	if (file_path.has_parent_path())
	{
		std::filesystem::create_directories(file_path.parent_path(), io_error);
		if (io_error)
		{
			error = "Could not create configuration directory: " +
			        io_error.message();
			return false;
		}
	}

	const std::filesystem::path temporary_path =
		std::filesystem::path(file_path.string() + ".tmp");
	std::ofstream output(temporary_path, std::ios::out | std::ios::trunc);
	if (!output)
	{
		error = "Could not write " + temporary_path.string();
		return false;
	}

	output << "# QtreGBPaint configuration\n";
	output << "version=1\n";
	output << "theme=" << theme_name(theme) << '\n';
	output << "grid=" << (grid_enabled ? 1 : 0) << '\n';
	if (window.has_position)
	{
		output << "window.x=" << window.x << '\n';
		output << "window.y=" << window.y << '\n';
	}
	output << "window.width=" << window.width << '\n';
	output << "window.height=" << window.height << '\n';
	output << "window.maximized=" << (window.maximized ? 1 : 0) << '\n';
	if (!last_directory.empty())
	{
		output << "last_directory=" << std::quoted(last_directory) << '\n';
	}

	std::size_t written = 0;
	for (const std::string& recent : recent_files)
	{
		if (recent.empty())
		{
			continue;
		}
		output << "recent_file=" << std::quoted(recent) << '\n';
		if (++written >= kMaxRecentFiles)
		{
			break;
		}
	}

	output.flush();
	if (!output)
	{
		error = "Could not write " + temporary_path.string();
		output.close();
		std::filesystem::remove(temporary_path, io_error);
		return false;
	}
	output.close();

	std::filesystem::rename(temporary_path, file_path, io_error);
	if (io_error)
	{
		std::filesystem::remove(temporary_path, io_error);
		error = "Could not replace " + file_path.string();
		return false;
	}

	return true;
}

bool AppConfig::save_default(std::string& error) const
{
	const std::string path = default_path();
	if (path.empty())
	{
		error = "No configuration path (HOME is not set)";
		return false;
	}
	return save(path, error);
}
