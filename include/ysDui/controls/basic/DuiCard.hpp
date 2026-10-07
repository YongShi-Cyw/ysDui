/**
 * 文件名：DuiCard.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：提供带标题和内容区域的卡片容器。
 */
#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

struct DuiCardPadding final {
    int left{16};
    int top{12};
    int right{16};
    int bottom{12};
};

class DuiCard final : public core::Control, public render::DuiRenderable {
public:
    DuiCard();
    ~DuiCard() override;
    DuiCard(const DuiCard&) = delete;
    DuiCard& operator=(const DuiCard&) = delete;
    DuiCard(DuiCard&&) noexcept;
    DuiCard& operator=(DuiCard&&) noexcept;

    void SetTitle(std::string title);
    [[nodiscard]] const std::string& Title() const;
    void SetSubtitle(std::string subtitle);
    [[nodiscard]] const std::string& Subtitle() const;
    void SetPadding(DuiCardPadding padding);
    [[nodiscard]] DuiCardPadding Padding() const;
    void SetCornerRadius(int pixels);
    [[nodiscard]] int CornerRadius() const;
    void SetHeaderHeight(int pixels);
    [[nodiscard]] int HeaderHeight() const;
    void SetContent(std::unique_ptr<core::Control> content);
    [[nodiscard]] core::Control* Content() const;
    [[nodiscard]] core::Rect ContentRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> card_;
};

} // namespace ysDui::controls::basic
