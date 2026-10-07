/**
 * 文件名：DuiRatingControl.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-05
 * 用途：声明星级评分控件——按整星选择评分，支持只读展示与悬停预览。
 */
#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

/**
 * 星级评分控件。
 * 取值为整数星数（0 表示未评分）。悬停时预览高亮，可设置为只读用于展示已有评分。
 */
class DuiRatingControl final : public core::Control, public render::DuiRenderable {
public:
    DuiRatingControl();
    ~DuiRatingControl() override;
    DuiRatingControl(const DuiRatingControl&) = delete;
    DuiRatingControl& operator=(const DuiRatingControl&) = delete;
    DuiRatingControl(DuiRatingControl&&) noexcept;
    DuiRatingControl& operator=(DuiRatingControl&&) noexcept;

    /** 设置总星数，至少 1；当前值会被钳制到新上限。 */
    void SetMaxRating(int value);
    [[nodiscard]] int MaxRating() const;
    /** 设置当前星数；越界时钳制到 [0, MaxRating]。 */
    void SetValue(int value, bool notify = false);
    [[nodiscard]] int Value() const;
    /** 设为只读：不响应点击/键盘，但保留悬停高亮。 */
    void SetReadOnly(bool readOnly);
    [[nodiscard]] bool ReadOnly() const;
    /** 设置单颗星的边长（逻辑像素）。 */
    void SetStarSize(int pixels);
    [[nodiscard]] int StarSize() const;
    /** 设置星与星的间距（逻辑像素）。 */
    void SetStarGap(int pixels);
    [[nodiscard]] int StarGap() const;
    void SetValueChangedHandler(std::function<void(int)> handler);

    /** @return 指定序号（0 基）星星的矩形；越界时为空矩形。 */
    [[nodiscard]] core::Rect StarRect(int index) const;
    /** @return 坐标对应的星序号（0 基）；未命中任何星时返回 -1。 */
    [[nodiscard]] int StarFromPoint(core::Point point) const;
    /** @return 当前参与绘制的星数（悬停预览优先于实际取值）。 */
    [[nodiscard]] int DisplayedValue() const;

    [[nodiscard]] core::Size DesiredSize() const override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    class Impl;
    std::unique_ptr<Impl> rating_;
};

} // namespace ysDui::controls::input
