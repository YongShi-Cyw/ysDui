/**
 * 文件名：DuiWin32TextRenderer.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：声明 Win32 私有 DirectWrite 文本布局、测量与绘制实现。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/render/DuiTextMetrics.hpp"

namespace ysDui::platform::win32 {

class DuiWin32TextRenderer final
{
public:
    DuiWin32TextRenderer(std::uintptr_t deviceContext, int dpi);
    ~DuiWin32TextRenderer();

    DuiWin32TextRenderer(const DuiWin32TextRenderer&) = delete;
    DuiWin32TextRenderer& operator=(const DuiWin32TextRenderer&) = delete;
    DuiWin32TextRenderer(DuiWin32TextRenderer&&) noexcept;
    DuiWin32TextRenderer& operator=(DuiWin32TextRenderer&&) noexcept;

    void DrawText(std::string_view text, core::Rect bounds,
                  const render::DuiTextStyle& style,
                  render::DuiTextAlignment alignment, bool wordWrap);

    [[nodiscard]] render::DuiTextMetrics MeasureText(
        std::string_view text, const render::DuiTextStyle& style,
        const render::DuiTextMeasureOptions& options) const;

private:
    struct Impl;
    std::unique_ptr<Impl> renderer_;
};

} // namespace ysDui::platform::win32
