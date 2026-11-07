#ifndef QTREGBPAINT_PAINT_BACKEND_H
#define QTREGBPAINT_PAINT_BACKEND_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ibackend.h"
#include "paint/palette.h"
#include "paint/indexed_image.h"
#include "tools/base_tool.h"

#include "imgui.h"

struct ToolButton
{
    std::string name;
    std::shared_ptr<ToolBase> tool;
    PaintCore::CursorType cursor = PaintCore::CursorType::Arrow;
    ImTextureID iconNormal  = 0;
    ImTextureID iconHover   = 0;
    ImTextureID iconPressed = 0;
};

class PaintBackend
{
public:
    PaintBackend(IGuiBackend* backend);
    ~PaintBackend();

    PaintBackend(const PaintBackend&) = delete;
    PaintBackend& operator=(const PaintBackend&) = delete;

    void addButton(const std::string& name,
                   std::shared_ptr<ToolBase> tool,
                   PaintCore::CursorType cursor,
                   const std::string& iconNormalPath,
                   const std::string& iconHoverPath,
                   const std::string& iconPressedPath,
                   const Palette& palette);

    const std::vector<ToolButton>& buttons() const { return m_buttons; }

    IGuiBackend* backend() const { return m_backend; }
    const Palette& iconPalette() const { return m_icon_palette; }

    const std::string& activeButton() const { return m_activeButton; }
    PaintCore::CursorType activeCursor() const { return m_activeCursor; }
    ToolBase* selectButton(const std::string& name);

    bool loadCursor(const std::string& buttonName, const std::string& path,
                    std::string& error);

    bool loadOptionIcon(const std::string& key, const std::string& sheetPath,
                        int x, int y, int width, int height);
    ImTextureID optionIcon(const std::string& key) const;

    ImTextureID loadIcon(const std::string& path, const Palette& palette);

private:
    IGuiBackend* m_backend;
    Palette m_icon_palette = Palette::grayscale(3);
    Palette m_option_icon_palette = Palette::grayscale(3);
    std::vector<ToolButton> m_buttons;
    std::vector<void*> m_textures;
    std::unordered_map<std::string, ImTextureID> m_option_icons;
    std::shared_ptr<ToolBase> m_activeTool;
    std::string m_activeButton;
    PaintCore::CursorType m_activeCursor = PaintCore::CursorType::Arrow;
    ImTextureID m_defaultIcon = 0;

    ImTextureID createIconTexture(const IndexedImage& image, int x, int y,
                                  int width, int height,
                                  const Palette& palette);
};

#endif
