// Headless tests for the file state and New/Open/Save operations exposed by
// PaintBackend. No SDL window is needed: the backend is constructed with a null
// IGuiBackend.
// Run with `make -C tests` or `make test` from the repository root.

#include "doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

#include "core/color.h"
#include "core/rect.h"
#include "paint/indexed_image.h"
#include "paint/paint_backend.h"

using PaintCore::ColorIndex;
using PaintCore::Rect;

namespace
{
struct TempDir
{
	std::filesystem::path path;

	TempDir()
	{
		static int counter = 0;
		path = std::filesystem::temp_directory_path() /
			("qtre_file_session_" +
			 std::to_string(static_cast<long>(::getpid())) + "_" +
			 std::to_string(++counter));
		std::error_code error;
		std::filesystem::create_directories(path, error);
	}

	~TempDir()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}

	std::string file(const std::string& name) const
	{
		return (path / name).string();
	}
};

std::vector<ColorIndex> read_pixels(PaintBackend& backend, int width, int height)
{
	const ImageBuffer& image = backend.session().get_main_buffer();
	std::vector<ColorIndex> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
			       static_cast<std::size_t>(x)] = image.get_pixel(x, y);
		}
	}
	return pixels;
}
}

TEST_CASE("file session starts untitled and clean")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(4, 3, error));
	CHECK(error.empty());
	CHECK_EQ(backend.session().getWidth(), 4);
	CHECK_EQ(backend.session().getHeight(), 3);
	CHECK_EQ(backend.session().getBitsPerPixel(), 2);
	CHECK_FALSE(backend.has_file_path());
	CHECK_EQ(backend.file_display_name(), "Untitled");
	CHECK_FALSE(backend.is_modified());
	CHECK_EQ(backend.foreground_index(), 3);
	CHECK_EQ(backend.background_index(), 0);
}

TEST_CASE("savepoint follows undo and redo")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(4, 4, error));
	REQUIRE(backend.session().document().apply_pixel_patch(
		Rect{0, 0, 2, 2}, {1, 2, 3, 0}));
	CHECK(backend.is_modified());

	const std::string path = dir.file("savepoint.idx");
	REQUIRE(backend.save_document_as(path, error));
	CHECK(error.empty());
	CHECK(backend.has_file_path());
	CHECK_EQ(backend.file().path(), path);
	CHECK_EQ(backend.file_display_name(), "savepoint.idx");
	CHECK_FALSE(backend.is_modified());

	REQUIRE(backend.undo());
	CHECK(backend.is_modified());

	REQUIRE(backend.redo());
	CHECK_FALSE(backend.is_modified());
}

TEST_CASE("save, open and new round-trip the document")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(3, 2, error));
	REQUIRE(backend.session().document().apply_pixel_patch(
		Rect{0, 0, 3, 2}, {1, 2, 3, 0, 2, 1}));

	const std::string first = dir.file("first.idx");
	REQUIRE(backend.save_document_as(first, error));
	CHECK_FALSE(backend.is_modified());

	const IndexedImage saved = IndexedImage::loadFromFile(first);
	REQUIRE(saved.isValid());
	CHECK_EQ(saved.width(), 3);
	CHECK_EQ(saved.height(), 2);
	CHECK_EQ(saved.bitsPerPixel(), 2);
	CHECK(saved.data() == std::vector<ColorIndex>({1, 2, 3, 0, 2, 1}));

	// New clears the path and starts a clean canvas with the current bpp.
	REQUIRE(backend.new_document(5, 4, error));
	CHECK_FALSE(backend.has_file_path());
	CHECK_FALSE(backend.is_modified());
	CHECK_EQ(backend.session().getWidth(), 5);
	CHECK_EQ(backend.session().getHeight(), 4);
	CHECK_EQ(backend.session().getBitsPerPixel(), 2);
	const std::vector<ColorIndex> blank = read_pixels(backend, 5, 4);
	CHECK(std::all_of(blank.begin(), blank.end(),
	                  [](ColorIndex value) { return value == 0; }));

	// Open restores dimensions, pixels, path and the clean state.
	REQUIRE(backend.open_document(first, error));
	CHECK(error.empty());
	CHECK(backend.has_file_path());
	CHECK_EQ(backend.file().path(), first);
	CHECK_EQ(backend.session().getWidth(), 3);
	CHECK_EQ(backend.session().getHeight(), 2);
	CHECK_FALSE(backend.is_modified());
	CHECK(read_pixels(backend, 3, 2) ==
	      std::vector<ColorIndex>({1, 2, 3, 0, 2, 1}));
	CHECK_EQ(backend.palette_visual_count(), 4);

	// Save As updates the path and leaves both files on disk.
	const std::string second = dir.file("second.idx");
	REQUIRE(backend.save_document_as(second, error));
	CHECK_EQ(backend.file().path(), second);
	CHECK_FALSE(backend.is_modified());
	CHECK(std::filesystem::exists(first));
	CHECK(std::filesystem::exists(second));
}

TEST_CASE("recent files track opens and saves without duplicates")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	CHECK(backend.recent_files().empty());

	REQUIRE(backend.new_document(2, 2, error));
	const std::string first = dir.file("first.idx");
	REQUIRE(backend.save_document_as(first, error));
	REQUIRE_EQ(backend.recent_files().size(), 1u);
	CHECK_EQ(backend.recent_files()[0], first);

	const std::string second = dir.file("second.idx");
	REQUIRE(backend.save_document_as(second, error));
	REQUIRE_EQ(backend.recent_files().size(), 2u);
	CHECK_EQ(backend.recent_files()[0], second);
	CHECK_EQ(backend.recent_files()[1], first);

	// Re-saving first moves it to the front without duplicating it.
	REQUIRE(backend.save_document_as(first, error));
	REQUIRE_EQ(backend.recent_files().size(), 2u);
	CHECK_EQ(backend.recent_files()[0], first);

	// Failed operations do not touch the list.
	CHECK_FALSE(backend.open_document(dir.file("missing.idx"), error));
	CHECK_EQ(backend.recent_files().size(), 2u);

	// Opening a file also marks it as recent.
	REQUIRE(backend.open_document(second, error));
	REQUIRE_EQ(backend.recent_files().size(), 2u);
	CHECK_EQ(backend.recent_files()[0], second);

	// The list is capped at 10 entries.
	for (int index = 0; index < 12; ++index)
	{
		const std::string path =
			dir.file("recent_" + std::to_string(index) + ".idx");
		REQUIRE(backend.save_document_as(path, error));
	}
	REQUIRE_EQ(backend.recent_files().size(), 10u);
	CHECK_EQ(backend.recent_files().front(),
	         dir.file("recent_11.idx"));
	CHECK_EQ(backend.recent_files().back(),
	         dir.file("recent_2.idx"));
}

TEST_CASE("changing bits per pixel remaps colors and supports undo")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(4, 1, 2, error));
	REQUIRE(backend.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 4, 1}, {0, 1, 2, 3}));

	REQUIRE(backend.change_bits_per_pixel(8));
	CHECK_EQ(backend.session().getBitsPerPixel(), 8);
	CHECK_EQ(backend.palette().bitsPerPixel(), 8u);
	const ImageBuffer& image = backend.session().get_main_buffer();
	CHECK_EQ(image.get_pixel(0, 0), 0);
	CHECK_EQ(image.get_pixel(1, 0), 85);
	CHECK_EQ(image.get_pixel(2, 0), 170);
	CHECK_EQ(image.get_pixel(3, 0), 255);
	CHECK(backend.is_modified());

	// Undo restores both the raw indices and the palette size.
	REQUIRE(backend.undo());
	CHECK_EQ(backend.session().getBitsPerPixel(), 2);
	CHECK_EQ(backend.palette().bitsPerPixel(), 2u);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(0, 0), 0);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(1, 0), 1);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(2, 0), 2);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(3, 0), 3);

	// Redo reapplies the same mapping and the 8bpp palette.
	REQUIRE(backend.redo());
	CHECK_EQ(backend.session().getBitsPerPixel(), 8);
	CHECK_EQ(backend.palette().bitsPerPixel(), 8u);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(1, 0), 85);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(3, 0), 255);
}

TEST_CASE("changing bits per pixel preserves a visible transparent index")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(4, 1, 2, error));
	REQUIRE(backend.set_transparent_color(0));
	REQUIRE(backend.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 4, 1}, {0, 1, 2, 3}));

	REQUIRE(backend.change_bits_per_pixel(8));
	CHECK(backend.palette().is_transparent(0));
	CHECK_EQ(backend.palette().getColor(0).a, 0);
	CHECK_EQ(backend.palette().getStoredColor(0).a, 255);
	const ImageBuffer& image = backend.session().get_main_buffer();
	CHECK_EQ(image.get_pixel(0, 0), 0);
	CHECK_EQ(image.get_pixel(1, 0), 85);

	// Indices that are not visible in the target bpp are rejected.
	CHECK_FALSE(backend.change_bits_per_pixel(0));
	CHECK_FALSE(backend.change_bits_per_pixel(9));
	CHECK_FALSE(backend.change_bits_per_pixel(8));
}

TEST_CASE("recent files restored from configuration are sanitized")
{
	PaintBackend backend(nullptr);
	backend.set_recent_files({
		"/tmp/a.idx", "", "/tmp/b.idx", "/tmp/a.idx",
		"/tmp/c.idx", "/tmp/d.idx", "/tmp/e.idx", "/tmp/f.idx",
		"/tmp/g.idx", "/tmp/h.idx", "/tmp/i.idx", "/tmp/j.idx",
		"/tmp/k.idx", "/tmp/l.idx",
	});

	REQUIRE_EQ(backend.recent_files().size(), 10u);
	CHECK_EQ(backend.recent_files()[0], "/tmp/a.idx");
	CHECK_EQ(backend.recent_files()[1], "/tmp/b.idx");
	CHECK_EQ(backend.recent_files()[9], "/tmp/j.idx");
}

TEST_CASE("opening a recent entry keeps the path valid")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(2, 2, error));
	const std::string first = dir.file("first.idx");
	const std::string second = dir.file("second.idx");
	REQUIRE(backend.save_document_as(first, error));
	REQUIRE(backend.save_document_as(second, error));
	CHECK_EQ(backend.recent_files().size(), 2u);

	// This used to pass a reference to the vector element into open_document();
	// add_recent_file() erased that same element before copying it, leaving a
	// dangling string in the list.
	const std::string& recent = backend.recent_files()[1];
	REQUIRE(backend.open_document(recent, error));
	CHECK_EQ(backend.file().path(), first);
	REQUIRE_EQ(backend.recent_files().size(), 2u);
	CHECK_EQ(backend.recent_files()[0], first);
	CHECK_EQ(backend.recent_files()[1], second);
}

TEST_CASE("saving remembers the selected idx storage format")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(3, 2, 2, error));
	CHECK(backend.file_format() == IndexedFormat::Index);
	REQUIRE(backend.session().document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 3, 1}, {0, 1, 2}));

	const std::string path = dir.file("packed.idx");
	REQUIRE(backend.save_document_as(path, IndexedFormat::BinaryPacked,
	                                 error));
	CHECK(backend.file_format() == IndexedFormat::BinaryPacked);

	const IndexedImage saved = IndexedImage::loadFromFile(path);
	REQUIRE(saved.isValid());
	CHECK(saved.format() == IndexedFormat::BinaryPacked);

	// Save keeps the current format; Save As can change it.
	REQUIRE(backend.save_document(error));
	const IndexedImage saved_again = IndexedImage::loadFromFile(path);
	REQUIRE(saved_again.isValid());
	CHECK(saved_again.format() == IndexedFormat::BinaryPacked);

	REQUIRE(backend.save_document_as(path, IndexedFormat::Packed, error));
	CHECK(backend.file_format() == IndexedFormat::Packed);
	const IndexedImage packed = IndexedImage::loadFromFile(path);
	REQUIRE(packed.isValid());
	CHECK(packed.format() == IndexedFormat::Packed);

	// Opening selects the format stored in the file; New returns to Index.
	REQUIRE(backend.new_document(2, 2, error));
	CHECK(backend.file_format() == IndexedFormat::Index);
	REQUIRE(backend.open_document(path, error));
	CHECK(backend.file_format() == IndexedFormat::Packed);
}

TEST_CASE("saving commits a floating selection")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(4, 4, error));
	REQUIRE(backend.session().document().apply_pixel_patch(
		Rect{0, 0, 1, 1}, {1}));

	const std::string path = dir.file("selection.idx");
	REQUIRE(backend.save_document_as(path, error));
	CHECK_FALSE(backend.is_modified());

	REQUIRE(backend.session().begin_selection(Rect{0, 0, 2, 2}, 0));
	REQUIRE(backend.session().move_floating_region(1, 1));
	CHECK(backend.session().has_floating_region());
	CHECK(backend.is_modified());

	REQUIRE(backend.save_document(error));
	CHECK(error.empty());
	CHECK_FALSE(backend.session().has_floating_region());
	CHECK_FALSE(backend.is_modified());
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(0, 0), 0);
	CHECK_EQ(backend.session().get_main_buffer().get_pixel(1, 1), 1);

	const IndexedImage saved = IndexedImage::loadFromFile(path);
	REQUIRE(saved.isValid());
	CHECK_EQ(saved.data()[0], 0);
	CHECK_EQ(saved.data()[static_cast<std::size_t>(1) * 4 + 1], 1);
}

TEST_CASE("opening an 8bpp file updates bpp and palette")
{
	TempDir dir;
	const std::string path = dir.file("eight.idx");
	const std::vector<ColorIndex> pixels{255, 254, 0, 1, 2, 3, 4, 5};
	std::string error;
	REQUIRE(save_indexed_image(path, 4, 2, 8, pixels, error));

	PaintBackend backend(nullptr);
	REQUIRE(backend.open_document(path, error));
	CHECK(error.empty());
	CHECK_EQ(backend.session().getBitsPerPixel(), 8);
	CHECK_EQ(backend.palette_visual_count(), 32);
	CHECK_EQ(backend.palette_usable_count(), 256);
	CHECK_EQ(backend.foreground_index(), 255);
	CHECK_EQ(backend.background_index(), 0);
	CHECK(read_pixels(backend, 4, 2) == pixels);

	// Opening a file resets the palette transparency.
	CHECK(backend.set_transparent_color(255));
	CHECK(backend.has_transparent_color());
	REQUIRE(backend.open_document(path, error));
	CHECK_FALSE(backend.has_transparent_color());
}

TEST_CASE("new document selects bpp and palette capacity")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(5, 4, 1, error));
	CHECK_EQ(backend.session().getBitsPerPixel(), 1);
	CHECK_EQ(backend.palette_usable_count(), 2);
	CHECK_EQ(backend.palette_visual_count(), 2);
	CHECK_EQ(backend.foreground_index(), 1);
	CHECK_EQ(backend.background_index(), 0);
	const std::vector<ColorIndex> one_bit_pixels = read_pixels(backend, 5, 4);
	CHECK(std::all_of(one_bit_pixels.begin(), one_bit_pixels.end(),
	                  [](ColorIndex value) { return value == 0; }));

	REQUIRE(backend.new_document(5, 4, 8, error));
	CHECK_EQ(backend.session().getBitsPerPixel(), 8);
	CHECK_EQ(backend.palette_usable_count(), 256);
	CHECK_EQ(backend.palette_visual_count(), 32);
	CHECK_EQ(backend.foreground_index(), 255);
	CHECK_EQ(backend.background_index(), 0);
	const std::vector<ColorIndex> eight_bit_pixels = read_pixels(backend, 5, 4);
	CHECK(std::all_of(eight_bit_pixels.begin(), eight_bit_pixels.end(),
	                  [](ColorIndex value) { return value == 0; }));

	// Inverting only changes the colors; the default indices remain the same.
	backend.toggle_palette_inverted();
	REQUIRE(backend.new_document(2, 2, 8, error));
	CHECK_EQ(backend.foreground_index(), 255);
	CHECK_EQ(backend.background_index(), 0);
	const std::vector<ColorIndex> inverted_pixels = read_pixels(backend, 2, 2);
	CHECK(std::all_of(inverted_pixels.begin(), inverted_pixels.end(),
	                  [](ColorIndex value) { return value == 0; }));

	CHECK_FALSE(backend.new_document(4, 4, 0, error));
	CHECK_FALSE(backend.new_document(4, 4, 9, error));
	CHECK_EQ(backend.session().getBitsPerPixel(), 8);
}

TEST_CASE("palette changes keep foreground and background indices")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(2, 2, 4, error));
	CHECK_EQ(backend.foreground_index(), 15);
	CHECK_EQ(backend.background_index(), 0);

	backend.set_foreground_index(5);
	backend.set_background_index(10);
	const PaintCore::Color old_fg = backend.palette().getColor(5);

	backend.select_palette(PaletteMode::Red);
	CHECK_EQ(backend.palette_mode(), PaletteMode::Red);
	CHECK_EQ(backend.foreground_index(), 5);
	CHECK_EQ(backend.background_index(), 10);

	const PaintCore::Color red_fg = backend.palette().getColor(5);
	const PaintCore::Color red_bg = backend.palette().getColor(10);
	CHECK((red_fg.r != old_fg.r || red_fg.g != old_fg.g ||
	       red_fg.b != old_fg.b));

	backend.toggle_palette_inverted();
	CHECK(backend.palette_inverted());
	CHECK_EQ(backend.foreground_index(), 5);
	CHECK_EQ(backend.background_index(), 10);

	// Inverting a 16-color palette maps index 5 to the former index 10.
	const PaintCore::Color inverted_fg = backend.palette().getColor(5);
	CHECK_EQ(inverted_fg.r, red_bg.r);
	CHECK_EQ(inverted_fg.g, red_bg.g);
	CHECK_EQ(inverted_fg.b, red_bg.b);
}

TEST_CASE("palette transparency is independent from palette colors")
{
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(2, 2, 8, error));
	CHECK_FALSE(backend.has_transparent_color());

	// Index 255 is the last visible swatch at 8bpp and can be transparent.
	CHECK(backend.set_transparent_color(255));
	REQUIRE(backend.transparent_index().has_value());
	CHECK_EQ(*backend.transparent_index(), 255);
	CHECK(backend.palette().is_transparent(255));
	CHECK_EQ(backend.palette().getColor(255).a, 0);
	// Palette transparency is not a document change.
	CHECK_FALSE(backend.is_modified());
	CHECK_FALSE(backend.can_undo());
	// Only the sampled swatches are selectable as transparent.
	CHECK_FALSE(backend.set_transparent_color(254));

	// Palette mode and inversion keep the transparent index; only the colors
	// of the swatches change.
	backend.select_palette(PaletteMode::Red);
	REQUIRE(backend.transparent_index().has_value());
	CHECK_EQ(*backend.transparent_index(), 255);
	CHECK_EQ(backend.palette().getColor(255).a, 0);

	backend.toggle_palette_inverted();
	REQUIRE(backend.transparent_index().has_value());
	CHECK_EQ(*backend.transparent_index(), 255);
	CHECK_EQ(backend.palette().getColor(255).a, 0);

	// New resets transparency; the stored swatch color becomes opaque again.
	REQUIRE(backend.new_document(2, 2, 8, error));
	CHECK_FALSE(backend.has_transparent_color());
	CHECK_EQ(backend.palette().getColor(255).a, 255);

	// Transparent Foreground uses the current foreground (index 16 is one of
	// the sampled 8bpp swatches).
	backend.set_foreground_index(16);
	CHECK(backend.set_transparent_foreground());
	REQUIRE(backend.transparent_index().has_value());
	CHECK_EQ(*backend.transparent_index(), 16);
	backend.clear_transparent_color();
	CHECK_FALSE(backend.has_transparent_color());
	CHECK_EQ(backend.palette().getColor(16).a, 255);

	// At 6bpp only the sampled swatches can be transparent.
	REQUIRE(backend.new_document(2, 2, 6, error));
	CHECK_FALSE(backend.set_transparent_color(61));
	CHECK(backend.set_transparent_color(63));
	REQUIRE(backend.transparent_index().has_value());
	CHECK_EQ(*backend.transparent_index(), 63);
}

TEST_CASE("file operations reject invalid input without touching the document")
{
	TempDir dir;
	PaintBackend backend(nullptr);
	std::string error;

	REQUIRE(backend.new_document(2, 2, error));
	const std::string before = dir.file("not_written.idx");

	CHECK_FALSE(backend.save_document(error));
	CHECK_FALSE(error.empty());

	CHECK_FALSE(backend.open_document(dir.file("missing.idx"), error));
	CHECK_FALSE(error.empty());
	CHECK_EQ(backend.session().getWidth(), 2);
	CHECK_FALSE(backend.is_modified());

	CHECK_FALSE(backend.new_document(0, 4, error));
	CHECK_FALSE(error.empty());
	CHECK_EQ(backend.session().getWidth(), 2);

	CHECK_FALSE(backend.save_document_as(dir.file("missing_dir/file.idx"), error));
	CHECK_FALSE(error.empty());
	CHECK_FALSE(backend.has_file_path());
	CHECK_FALSE(std::filesystem::exists(before));
}
