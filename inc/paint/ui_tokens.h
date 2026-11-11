#ifndef QTREGBPAINT_PAINT_UI_TOKENS_H
#define QTREGBPAINT_PAINT_UI_TOKENS_H

// Small, shared design tokens for the ImGui frontend. They describe spacing,
// sizes and alphas used by custom-drawn widgets; theme colors live in
// PaintUi::Roles. This header intentionally has no ImGui dependency.
namespace PaintUi
{
namespace Space
{
inline constexpr float kXxs = 2.0f;
inline constexpr float kXs = 4.0f;
inline constexpr float kSm = 8.0f;
inline constexpr float kMd = 12.0f;
inline constexpr float kLg = 16.0f;
inline constexpr float kXl = 24.0f;
}

namespace Size
{
inline constexpr float kToolButton = 32.0f;
inline constexpr float kToolOptionsHeight = 116.0f;
inline constexpr float kToolOptionRow = 24.0f;
inline constexpr float kToolOptionIconWidth = 60.0f;
inline constexpr float kToolOptionIconHeight = 16.0f;
inline constexpr float kPaletteColor = 50.0f;
inline constexpr float kSwatch = 28.0f;
inline constexpr float kSwatchGap = 6.0f;
inline constexpr float kSwatchBevel = 2.0f;
inline constexpr float kDialogButton = 80.0f;
}

namespace Alpha
{
inline constexpr float kHover = 0.08f;
inline constexpr float kPressed = 0.14f;
inline constexpr float kSelected = 0.65f;
}
} // namespace PaintUi

#endif
