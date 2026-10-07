#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiOverlayControl.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/render/DuiTextMeasurer.hpp"

namespace ysDui::controls::basic {

/** 消息类型：决定浅色外观下的配色与默认图标形状。 */
enum class DuiToastType
{
    Information, // 中性提示：普通状态反馈
    Success,     // 成功：操作完成
    Warning,     // 警告：需注意但不阻断
    Error,       // 错误：操作失败
};

/**
 * 消息外观。
 * Dark 为深色底白字的原有样式，需调用方自备图标与底色；
 * Light 为按类型着色的浅色底（对应 Element Plus Message 的默认观感），
 * 配色由主题在绘制时按类型混出，无需调用方指定颜色。
 */
enum class DuiToastAppearance
{
    Dark,
    Light,
};

/** 消息相对可用区域边缘的停靠边。 */
enum class DuiToastPlacement
{
    Top,
    Bottom,
};

class DuiToast final : public core::DuiOverlayControl, public render::DuiRenderable {
public:
    DuiToast();
    ~DuiToast() override;
    DuiToast(const DuiToast&) = delete;
    DuiToast& operator=(const DuiToast&) = delete;
    DuiToast(DuiToast&&) = delete;
    DuiToast& operator=(DuiToast&&) = delete;

    void SetAnimationClock(core::AnimationClock* clock);
    void Show(std::string text);
    void HideNow();
    [[nodiscard]] bool Active() const;
    [[nodiscard]] double Opacity() const;
    [[nodiscard]] const std::string& Text() const;
    /** 设置停留时长；传 0 表示不自动关闭，需显式 HideNow 或点击关闭按钮。 */
    void SetDurationMilliseconds(int milliseconds);
    [[nodiscard]] int DurationMilliseconds() const;
    void SetFadeMilliseconds(int milliseconds);
    [[nodiscard]] int FadeMilliseconds() const;
    void SetTextMeasurer(render::DuiTextMeasurer* measurer);
    void SetTextStyle(render::DuiTextStyle style);
    /** 设置固定底色；设置后覆盖外观（Light/Dark）的自动配色。 */
    void SetBackgroundColor(core::Color color);
    /** 设置圆角半径；默认为 0（直角），需要胶囊外观时传正值。 */
    void SetCornerRadius(int pixels);
    /** 设置自定义图标图像；空指针表示使用类型默认字形（浅色外观下）。 */
    void SetIcon(std::shared_ptr<const render::DuiImage> image);
    void SetIconSize(int pixels);
    void SetIconGap(int pixels);
    /** 设置距停靠边的偏移；配合 SetPlacement 使用。 */
    void SetEdgeOffset(int pixels);
    /** 保留原名，等价于 SetEdgeOffset（历史调用点使用）。 */
    void SetTopOffset(int pixels);
    [[nodiscard]] int EdgeOffset() const;
    /** 设置停靠边；Bottom 时从可用区域底边向上计算。 */
    void SetPlacement(DuiToastPlacement placement);
    [[nodiscard]] DuiToastPlacement Placement() const;
    void SetType(DuiToastType type);
    [[nodiscard]] DuiToastType Type() const;
    void SetAppearance(DuiToastAppearance appearance);
    [[nodiscard]] DuiToastAppearance Appearance() const;
    /** 设置是否显示右上角关闭按钮。 */
    void SetShowClose(bool showClose);
    [[nodiscard]] bool ShowClose() const;
    /** 注册关闭回调（自动消失或点击关闭按钮时触发）。 */
    void SetClosedHandler(std::function<void()> handler);
    /** @return 关闭按钮命中矩形；未启用时为空矩形。 */
    [[nodiscard]] core::Rect CloseRect() const;
    void SetMaxWidth(int pixels);
    [[nodiscard]] core::Rect ContentRect() const;
    void Layout(core::Rect availableBounds);
    /** 仅当坐标落在关闭按钮上时才声明命中，其余位置继续穿透给下层控件。 */
    [[nodiscard]] core::Control* HitTest(core::Point point) override;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;
    [[nodiscard]] static int MeasureWidth(int textPixels, bool hasIcon, int iconPixels, int iconGapPixels);
    [[nodiscard]] static std::string ApplyEllipsis(std::string_view text, int maximumCharacters);

private:
    void StartFadeIn(int generation);
    void StartHold(int generation);
    void StartFadeOut(int generation);
    void FinishHide();
    void CancelAnimation();

    class Impl;
    std::unique_ptr<Impl> toast_;
};

} // namespace ysDui::controls::basic
