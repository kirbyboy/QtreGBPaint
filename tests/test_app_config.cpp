#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "paint/app_config.h"

namespace
{
class TempDir
{
public:
	TempDir()
	{
		const auto stamp =
			std::chrono::steady_clock::now().time_since_epoch().count();
		m_path = std::filesystem::temp_directory_path() /
		         ("qtregbpaint_config_" + std::to_string(stamp));
		std::filesystem::create_directories(m_path);
	}

	~TempDir()
	{
		std::error_code error;
		std::filesystem::remove_all(m_path, error);
	}

	std::string file(const std::string& name) const
	{
		return (m_path / name).string();
	}

private:
	std::filesystem::path m_path;
};

void write_file(const std::string& path, const std::string& contents)
{
	std::ofstream output(path, std::ios::out | std::ios::trunc);
	output << contents;
}

std::string read_file(const std::string& path)
{
	std::ifstream input(path);
	return std::string(std::istreambuf_iterator<char>(input),
	                   std::istreambuf_iterator<char>());
}
}

TEST_CASE("AppConfig defaults when the file does not exist")
{
	TempDir dir;
	const AppConfig config = AppConfig::load(dir.file("missing.ini"));

	CHECK(config.theme == AppTheme::Light);
	CHECK_EQ(config.language, "system");
	CHECK(config.grid_enabled);
	CHECK_EQ(config.window.width, 800);
	CHECK_EQ(config.window.height, 600);
	CHECK_FALSE(config.window.has_position);
	CHECK_FALSE(config.window.maximized);
	CHECK(config.recent_files.empty());
}

TEST_CASE("AppConfig round-trips theme, grid, window and recent files")
{
	TempDir dir;
	const std::string path = dir.file("nested/config.ini");

	AppConfig config;
	config.theme = AppTheme::Classic;
	config.language = "es";
	config.grid_enabled = false;
	config.window = PaintCore::WindowGeometry{-40, 25, 1000, 700, true, true};
	config.last_directory = "/tmp/a folder/with spaces";
	config.recent_files = {
		"/tmp/a file/with spaces.idx",
		"/tmp/a=b/c.idx",
		"/tmp/plain.idx",
	};

	std::string error;
	REQUIRE(config.save(path, error));
	CHECK(error.empty());

	const AppConfig loaded = AppConfig::load(path);
	CHECK(loaded.theme == AppTheme::Classic);
	CHECK_EQ(loaded.language, "es");
	CHECK_FALSE(loaded.grid_enabled);
	CHECK_EQ(loaded.window.x, -40);
	CHECK_EQ(loaded.window.y, 25);
	CHECK_EQ(loaded.window.width, 1000);
	CHECK_EQ(loaded.window.height, 700);
	CHECK(loaded.window.maximized);
	CHECK(loaded.window.has_position);
	CHECK_EQ(loaded.last_directory, "/tmp/a folder/with spaces");
	REQUIRE_EQ(loaded.recent_files.size(), 3u);
	CHECK_EQ(loaded.recent_files[0], "/tmp/a file/with spaces.idx");
	CHECK_EQ(loaded.recent_files[1], "/tmp/a=b/c.idx");
	CHECK_EQ(loaded.recent_files[2], "/tmp/plain.idx");

	const std::string contents = read_file(path);
	CHECK(contents.find("theme=classic") != std::string::npos);
	CHECK(contents.find("language=es") != std::string::npos);
	CHECK(contents.find("grid=0") != std::string::npos);
	CHECK(contents.find("window.maximized=1") != std::string::npos);
	CHECK(contents.find("last_directory=\"/tmp/a folder/with spaces\"") !=
	      std::string::npos);
	CHECK(contents.find("recent_file=\"/tmp/a file/with spaces.idx\"") !=
	      std::string::npos);
}

TEST_CASE("AppConfig ignores malformed values and sanitizes recent files")
{
	TempDir dir;
	const std::string path = dir.file("config.ini");
	write_file(path,
	           "# comment\n"
	           "; another comment\n"
	           "theme=purple\n"
	           "grid=maybe\n"
	           "window.x=not-a-number\n"
	           "window.y=10\n"
	           "window.width=0\n"
	           "window.height=-4\n"
	           "unknown_key=anything\n"
	           "recent_file=\"/tmp/one.idx\"\n"
	           "recent_file=\"/tmp/one.idx\"\n"
	           "recent_file=\"/tmp/two.idx\"\n"
	           "recent_file=\"/tmp/three.idx\"\n"
	           "recent_file=\"/tmp/four.idx\"\n"
	           "recent_file=\"/tmp/five.idx\"\n"
	           "recent_file=\"/tmp/six.idx\"\n"
	           "recent_file=\"/tmp/seven.idx\"\n"
	           "recent_file=\"/tmp/eight.idx\"\n"
	           "recent_file=\"/tmp/nine.idx\"\n"
	           "recent_file=\"/tmp/ten.idx\"\n"
	           "recent_file=\"/tmp/eleven.idx\"\n");

	const AppConfig config = AppConfig::load(path);
	CHECK(config.theme == AppTheme::Light);
	CHECK(config.grid_enabled);
	CHECK_EQ(config.window.width, 800);
	CHECK_EQ(config.window.height, 600);
	CHECK_FALSE(config.window.has_position);
	REQUIRE_EQ(config.recent_files.size(), 10u);
	CHECK_EQ(config.recent_files.front(), "/tmp/one.idx");
	CHECK_EQ(config.recent_files.back(), "/tmp/ten.idx");
}

TEST_CASE("AppConfig drops paths with control characters")
{
	TempDir dir;
	const std::string path = dir.file("config.ini");
	std::string contents =
		"last_directory=\"/tmp/good\"\n"
		"recent_file=\"/tmp/one.idx\"\n"
		"recent_file=\"bad";
	contents.push_back('\x01');
	contents += "path.idx\"\n"
	            "recent_file=\"/tmp/two.idx\"\n";
	write_file(path, contents);

	const AppConfig config = AppConfig::load(path);
	CHECK_EQ(config.last_directory, "/tmp/good");
	REQUIRE_EQ(config.recent_files.size(), 2u);
	CHECK_EQ(config.recent_files[0], "/tmp/one.idx");
	CHECK_EQ(config.recent_files[1], "/tmp/two.idx");
}

TEST_CASE("AppConfig omits the position when it is not known")
{
	TempDir dir;
	const std::string path = dir.file("config.ini");

	AppConfig config;
	config.window = PaintCore::WindowGeometry{};
	config.window.maximized = true;

	std::string error;
	REQUIRE(config.save(path, error));

	const std::string contents = read_file(path);
	CHECK(contents.find("window.x=") == std::string::npos);
	CHECK(contents.find("window.y=") == std::string::npos);

	const AppConfig loaded = AppConfig::load(path);
	CHECK_FALSE(loaded.window.has_position);
	CHECK(loaded.window.maximized);
}

TEST_CASE("AppConfig save leaves no temporary file behind")
{
	TempDir dir;
	const std::string path = dir.file("config.ini");

	AppConfig config;
	std::string error;
	REQUIRE(config.save(path, error));
	CHECK(std::filesystem::exists(path));
	CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}
