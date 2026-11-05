// Tests headless del parser P2 de PPMImage: cabecera, dimensiones, bpp,
// cantidad de datos y rango de valores.
// Ejecutar con `make -C tests` o `make test` desde la raíz.

#include "doctest/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

#include "paint/ppm_image.h"

namespace
{
struct TempPpmFile
{
	std::string path;

	explicit TempPpmFile(const std::string& contents)
	{
		static int counter = 0;
		const std::string name = "qtre_ppm_test_" +
			std::to_string(static_cast<long>(::getpid())) + "_" +
			std::to_string(++counter) + ".p2";
		path = (std::filesystem::temp_directory_path() / name).string();

		std::ofstream out(path);
		out << contents;
	}

	~TempPpmFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

PPMImage load(const std::string& contents)
{
	const TempPpmFile file(contents);
	return PPMImage::loadFromFile(file.path);
}
}

TEST_CASE("PPMImage loads a valid P2 file")
{
	const PPMImage image = load("P2\n3 2\n2\n0 1 2 3 0 1\n");

	CHECK(image.isValid());
	CHECK_EQ(image.width(), 3);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.expectedSize(), 6u);
	CHECK_EQ(image.data().size(), 6u);
	CHECK_EQ(image.data()[0], 0);
	CHECK_EQ(image.data()[1], 1);
	CHECK_EQ(image.data()[2], 2);
	CHECK_EQ(image.data()[3], 3);
	CHECK_EQ(image.data()[4], 0);
	CHECK_EQ(image.data()[5], 1);
	CHECK(image.error().empty());
	CHECK(image.warning().empty());
}

TEST_CASE("PPMImage ignores extra pixel values with a warning")
{
	const PPMImage image = load("P2\n2 2\n3\n0 1 2 3 4 5\n");

	CHECK(image.isValid());
	CHECK_EQ(image.expectedSize(), 4u);
	CHECK_EQ(image.data().size(), 4u);
	CHECK_EQ(image.data()[0], 0);
	CHECK_EQ(image.data()[3], 3);
	CHECK(!image.warning().empty());
}

TEST_CASE("PPMImage rejects malformed headers")
{
	const PPMImage empty = load("");
	CHECK(!empty.isValid());
	CHECK(!empty.error().empty());

	const PPMImage bad_magic = load("P1\n2 2\n2\n0 1 2 3\n");
	CHECK(!bad_magic.isValid());
	CHECK(!bad_magic.error().empty());

	const PPMImage bad_width = load("P2\n0 2\n2\n0 1 2 3\n");
	CHECK(!bad_width.isValid());
	CHECK(!bad_width.error().empty());

	const PPMImage bad_height = load("P2\n2 0\n2\n0 1 2 3\n");
	CHECK(!bad_height.isValid());
	CHECK(!bad_height.error().empty());

	const PPMImage bad_bpp = load("P2\n2 2\n0\n0 1 2 3\n");
	CHECK(!bad_bpp.isValid());
	CHECK(!bad_bpp.error().empty());

	const PPMImage non_numeric = load("P2\nx 2\n2\n0 1 2 3\n");
	CHECK(!non_numeric.isValid());
	CHECK(!non_numeric.error().empty());
}

TEST_CASE("PPMImage rejects invalid or missing pixel data")
{
	const PPMImage out_of_range = load("P2\n2 2\n2\n0 1 2 256\n");
	CHECK(!out_of_range.isValid());
	CHECK(!out_of_range.error().empty());

	const PPMImage negative = load("P2\n2 2\n2\n0 1 2 -1\n");
	CHECK(!negative.isValid());
	CHECK(!negative.error().empty());

	const PPMImage insufficient = load("P2\n2 2\n2\n0 1 2\n");
	CHECK(!insufficient.isValid());
	CHECK(!insufficient.error().empty());
}

TEST_CASE("PPMImage reports a missing file")
{
	const std::string name = "qtre_ppm_test_missing_" +
		std::to_string(static_cast<long>(::getpid())) + ".p2";
	const std::string path = (std::filesystem::temp_directory_path() / name).string();
	std::error_code error;
	std::filesystem::remove(path, error);
	const PPMImage image = PPMImage::loadFromFile(path);

	CHECK(!image.isValid());
	CHECK(!image.error().empty());
}

TEST_CASE("PPMImage starts invalid and can be constructed in memory")
{
	PPMImage empty;
	CHECK(!empty.isValid());
	CHECK_EQ(empty.width(), 0);
	CHECK_EQ(empty.height(), 0);
	CHECK_EQ(empty.bitsPerPixel(), 0);
	CHECK_EQ(empty.expectedSize(), 0u);
	CHECK(empty.data().empty());

	const std::vector<std::uint8_t> pixels{0, 1, 2};
	PPMImage image(2, 2, 2, pixels);
	CHECK(image.isValid());
	CHECK_EQ(image.width(), 2);
	CHECK_EQ(image.height(), 2);
	CHECK_EQ(image.bitsPerPixel(), 2);
	CHECK_EQ(image.expectedSize(), 4u);
	CHECK_EQ(image.data().size(), 3u);
}
