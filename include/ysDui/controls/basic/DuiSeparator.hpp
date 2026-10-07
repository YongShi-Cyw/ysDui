#pragma once

#include <memory>
#include <string>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::basic {

/** 分割线中间文本的对齐方式（仅水平分割线有效）。 */
enum class DuiSeparatorTextAlign
{
    Left,   // 文本靠左，右侧保留线段。
    Center, // 文本居中，两侧都有线段。
    Right,  // 文本靠右，左侧保留线段。
};

class DuiSeparator final : public core::Control, public render::DuiRenderable {
public:
    enum class Orientation {
        Horizontal,
        Vertical,
    };

    DuiSeparator();
    ~DuiSeparator() override;
    DuiSeparator(const DuiSeparator&) = delete;
    DuiSeparator& operator=(const DuiSeparator&) = delete;
    DuiSeparator(DuiSeparator&&) noexcept;
    DuiSeparator& operator=(DuiSeparator&&) noexcept;

    void SetOrientation(Orientation orientation);
    [[nodiscard]] Orientation GetOrientation() const;
    void SetColor(core::Color color);
    [[nodiscard]] core::Color GetColor() const;
    void SetThickness(int pixels);
    [[nodiscard]] int GetThickness() const;
    void SetInset(int pixels);
    [[nodiscard]] int GetInset() const;

    /**
     * 设置线段中间的文本；空串表示纯分割线。
     * 说明：文本只画在水平分割线上——纵向分割线需要旋转文字，而画布只提供相似变换（缩放+平移）。
     * 带文本时高度不会自动撑开（与其他文本控件一致），请在布局或 Bounds 中给出足够高度。
     * @param text UTF-8 文本
     */
    void SetText(std::string text);
    /** @return 当前文本 */
    [[nodiscard]] const std::string& Text() const;
    /**
     * 设置文本对齐方式。
     * @param align 对齐；仅水平分割线有效
     */
    void SetTextAlign(DuiSeparatorTextAlign align);
    /** @return 当前文本对齐方式 */
    [[nodiscard]] DuiSeparatorTextAlign TextAlign() const;
    /**
     * 设置文本与两侧线段之间的间距。
     * @param pixels 间距像素；负值按 0 处理
     */
    void SetTextGap(int pixels);
    /** @return 当前文本间距 */
    [[nodiscard]] int TextGap() const;
    /**
     * 设置是否以虚线绘制。
     * @param dashed true 为虚线；仅水平分割线有效
     */
    void SetDashed(bool dashed);
    /** @return 是否虚线 */
    [[nodiscard]] bool Dashed() const;
    /**
     * 设置文本样式；颜色未显式设置时取主题的次要文本色。
     * @param style 文本样式
     */
    void SetTextStyle(render::DuiTextStyle style);
    /** @return 当前文本样式 */
    [[nodiscard]] const render::DuiTextStyle& TextStyle() const;

    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> separator_;
};

} // namespace ysDui::controls::basic
