#ifndef QTREGBPAINT_PAINT_BACKEND_H
#define QTREGBPAINT_PAINT_BACKEND_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ibackend.h"
#include "paint/document_file.h"
#include "paint/image_export.h"
#include "paint/palette.h"
#include "paint/indexed_image.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"

#include "imgui.h"

struct ToolButton
{
    std::string name;
    std::shared_ptr<ToolBase> tool;
    PaintCore::CursorType cursor = PaintCore::CursorType::Arrow;
    ImTextureID icon = 0;
};

class PaintBackend
{
public:
    PaintBackend(IGuiBackend* backend);
    ~PaintBackend();

    PaintBackend(const PaintBackend&) = delete;
    PaintBackend& operator=(const PaintBackend&) = delete;

    // Application state: the backend owns the document, history and floating
    // selection. The frontend only queries it and forwards user commands.
    PaintSession& session() { return m_session; }
    const PaintSession& session() const { return m_session; }

    const Palette& palette() const { return m_palette; }
    PaletteMode palette_mode() const { return m_palette_mode; }
    bool palette_inverted() const { return m_palette_inverted; }
    int palette_usable_count() const;
    int palette_visual_count() const;
    PaintCore::ColorIndex palette_visual_index(int slot) const;

    PaintCore::ColorIndex foreground_index() const { return m_foreground_index; }
    PaintCore::ColorIndex background_index() const { return m_background_index; }

    // Palette transparency: one visible palette index can be rendered with
    // alpha 0. It is a palette property, not a document/history change.
    std::optional<PaintCore::ColorIndex> transparent_index() const
    {
        return m_palette.transparent_index();
    }
    bool has_transparent_color() const
    {
        return m_palette.transparent_index().has_value();
    }
    bool set_transparent_color(PaintCore::ColorIndex index);
    bool set_transparent_foreground();
    void clear_transparent_color();

    void select_palette(PaletteMode mode);
    void toggle_palette_inverted();
    void set_foreground_index(PaintCore::ColorIndex index);
    void set_background_index(PaintCore::ColorIndex index);

    bool can_undo() const;
    bool can_redo() const;
    bool undo();
    bool redo();
    bool resize_canvas(int width, int height);
    bool change_bits_per_pixel(int bits_per_pixel);
    bool transform_canvas(PaintCore::ImageTransform transform);

    // Applies a pending pick from the active color-picker tool. Left picks
    // update the foreground, right picks the background, and the previous
    // editing tool is selected again.
    bool apply_pending_color_pick();

    // File state and operations. New/Open/Save are synchronous; the frontend
    // owns the file dialogs and just forwards paths here.
    const DocumentFile& file() const { return m_file; }
    bool has_file_path() const { return m_file.has_path(); }
    IndexedFormat file_format() const { return m_file_format; }
    std::string file_display_name() const { return m_file.display_name(); }
    bool is_modified() const;
    bool new_document(int width, int height, std::string& error);
    bool new_document(int width, int height, int bits_per_pixel,
                      std::string& error);
    bool open_document(const std::string& path, std::string& error);
    bool save_document(std::string& error);
    bool save_document_as(const std::string& path, std::string& error);
    bool save_document_as(const std::string& path, IndexedFormat format,
                          std::string& error);

    // Writes a copy in the requested image format. Exporting never updates the
    // .idx savepoint or the recent-files list, but it does commit an active
    // floating region/pending edit so the exported copy matches the canvas.
    bool export_image(const std::string& path, ImageExportFormat format,
                      std::string& error);

    // Most recently opened/saved files, newest first, capped at 10. The menu
    // uses it to build the recent-files section; AppConfig persists it.
    const std::vector<std::string>& recent_files() const
    {
        return m_recent_files;
    }
    // Replaces the in-memory list with the one loaded from AppConfig.
    void set_recent_files(const std::vector<std::string>& files);

    void addButton(const std::string& name,
                   std::shared_ptr<ToolBase> tool,
                   PaintCore::CursorType cursor,
                   const std::string& iconPath,
                   const Palette& palette);

    const std::vector<ToolButton>& buttons() const { return m_buttons; }

    IGuiBackend* backend() const { return m_backend; }
    const Palette& iconPalette() const { return m_icon_palette; }

    const std::string& activeButton() const { return m_activeButton; }
    PaintCore::CursorType activeCursor() const { return m_activeCursor; }
    ToolBase* selectButton(const std::string& name);

    bool loadCursor(const std::string& buttonName, const std::string& path,
                    std::string& error);
    bool loadCursor(PaintCore::CursorType cursor, const std::string& path,
                    std::string& error);
    bool setCursorImage(PaintCore::CursorType cursor,
                        const PaintCore::CursorImage& image);

    bool loadOptionIcon(const std::string& key, const std::string& sheetPath,
                        int x, int y, int width, int height);
    ImTextureID optionIcon(const std::string& key) const;

    ImTextureID loadIcon(const std::string& path, const Palette& palette);

private:
    IGuiBackend* m_backend;
    PaintSession m_session;
    DocumentFile m_file;

    Palette m_palette{2};
    PaletteMode m_palette_mode = PaletteMode::Dmg;
    bool m_palette_inverted = false;
    PaintCore::ColorIndex m_foreground_index = 3;
    PaintCore::ColorIndex m_background_index = 0;

    Palette m_icon_palette = Palette::grayscale(3);
    Palette m_option_icon_palette = Palette::grayscale(3);
    std::vector<ToolButton> m_buttons;
    std::vector<void*> m_textures;
    std::unordered_map<std::string, ImTextureID> m_option_icons;
    std::shared_ptr<ToolBase> m_activeTool;
    std::string m_activeButton;
    // Last tool that edits the document. Selection and Pick Color are
    // transient/non-editing tools and do not overwrite it.
    std::string m_last_edit_button;
    std::vector<std::string> m_recent_files;
    IndexedFormat m_file_format = IndexedFormat::Index;
    PaintCore::CursorType m_activeCursor = PaintCore::CursorType::Arrow;
    ImTextureID m_defaultIcon = 0;

    int palette_bits_per_pixel() const;
    void regenerate_palette();
    void apply_default_colors();
    void apply_tool_colors();
    void refresh_palette_for_document_bpp();
    void add_recent_file(std::string path);

    ImTextureID createIconTexture(const IndexedImage& image, int x, int y,
                                  int width, int height,
                                  const Palette& palette);
};

#endif
