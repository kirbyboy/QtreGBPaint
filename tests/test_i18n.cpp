#include "doctest/doctest.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include "paint/i18n.h"

namespace
{
std::string language_directory()
{
	const std::filesystem::path source(__FILE__);
	const std::filesystem::path root =
		source.parent_path().parent_path();
	if (std::filesystem::exists(root / "lang" / "es.po"))
	{
		return (root / "lang").string();
	}
	return "../lang";
}
}

TEST_CASE("i18n falls back to English without a catalog")
{
	i18n::set_language_directory("");
	i18n::set_locale("en");

	CHECK_EQ(i18n::resolved_locale(), "en");
	CHECK_EQ(std::string(i18n::tr("Open")), "Open");
	CHECK_EQ(std::string(i18n::tr("Not translated")), "Not translated");
}

TEST_CASE("i18n loads the Spanish catalog")
{
	i18n::set_language_directory(language_directory());
	i18n::set_locale("es");

	CHECK_EQ(i18n::resolved_locale(), "es");
	CHECK_EQ(std::string(i18n::tr("Open")), "Abrir");
	CHECK_EQ(std::string(i18n::tr("Save As...")), "Guardar como...");
	CHECK_EQ(std::string(i18n::tr("Colors:")), "Colores:");
	CHECK_EQ(std::string(i18n::tr("Save changes to \"%s\" before closing?")),
	         "¿Guardar los cambios de \"%s\" antes de cerrar?");
	CHECK_EQ(std::string(i18n::tr("Not translated")), "Not translated");

	i18n::set_locale("en");
	CHECK_EQ(i18n::resolved_locale(), "en");
	CHECK_EQ(std::string(i18n::tr("Open")), "Open");
}

TEST_CASE("i18n detects Spanish from LANGUAGE")
{
	const char* previous = std::getenv("LANGUAGE");
	const bool had_previous = previous != nullptr;
	const std::string saved = had_previous ? previous : "";

	setenv("LANGUAGE", "es_ES.UTF-8", 1);
	CHECK_EQ(i18n::detect_system_locale(), "es");

	if (had_previous)
	{
		setenv("LANGUAGE", saved.c_str(), 1);
	}
	else
	{
		unsetenv("LANGUAGE");
	}
}
