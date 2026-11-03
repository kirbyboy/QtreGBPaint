#ifndef QTREGBPAINT_PAINT_BACKEND_H
#define QTREGBPAINT_PAINT_BACKEND_H

#include <memory>
#include <string>
#include <vector>

#include "core/ibackend.h"
#include "paint/palette.h"
#include "paint/ppm_image.h"
#include "tools/base_tool.h"

#include "imgui.h"

struct ToolButton
{
    std::string name;
    std::shared_ptr<ToolBase> tool;
    ImTextureID iconNormal  = 0;
    ImTextureID iconHover   = 0;
    ImTextureID iconPressed = 0;
};

class PaintBackend
{
public:
    PaintBackend(IGuiBackend* backend);

    void addButton(const std::string& name,
                   std::shared_ptr<ToolBase> tool,
                   const std::string& iconNormalPath,
                   const std::string& iconHoverPath,
                   const std::string& iconPressedPath,
                   const Palette& palette);

    const std::vector<ToolButton>& buttons() const { return m_buttons; }

    //ToolBase* activeTool() const { return m_activeTool; }
    const std::string& activeButton() const { return m_activeButton; }
    ToolBase* selectButton(const std::string& name);

    ImTextureID loadIcon(const std::string& path, const Palette& palette);

private:
    IGuiBackend* m_backend;
    std::vector<ToolButton> m_buttons;
    std::vector<void*> m_textures;
    std::shared_ptr<ToolBase> m_activeTool;
    std::string m_activeButton;
    ImTextureID m_defaultIcon = 0;
};

#endif
