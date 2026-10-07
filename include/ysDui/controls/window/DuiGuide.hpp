/**
 * 文件名：DuiGuide.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：新手引导控件：绘制遮罩并高亮目标元素，逐步在其旁显示标题、说明与操作按钮。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::controls::window {

/** 引导框类型。 */
enum class DuiGuideMode
{
    Popup,  // 锚定在高亮元素旁的气泡（默认）
    Dialog, // 居中显示的弹窗，不绘制高亮框
};

/** 单个引导步骤。 */
struct DuiGuideStep final
{
    /**
     * 高亮元素矩形，坐标相对本控件左上角。
     * Dialog 模式不绘制高亮，可留空。
     */
    core::Rect element;
    std::string title; // 引导框标题
    std::string body;  // 引导框说明正文
    /** 引导框相对高亮元素的放置方向；Dialog 模式忽略。 */
    ui::DuiPopupPlacement placement{ui::DuiPopupPlacement::Below};
    /** 高亮框相对元素的外扩内边距；负数表示使用引导级默认值。 */
    int highlightPadding{-1};
};

/** 引导按钮的文案与样式。 */
struct DuiGuideButtonProps final
{
    std::string text; // 按钮文案；为空时沿用该按钮已设置的文案
    bool primary{};   // true 使用主色按钮，false 使用默认按钮
};

/**
 * 新手引导控件。
 *
 * 作为覆盖层使用：把它加为**宿主根控件（或页面容器）的最后一个子节点**，
 * 并在父容器 `Layout` 末尾按同一矩形调用本控件的 `Layout`，即可覆盖整个可视区。
 * 与 `DuiToastCenter` 相同，父容器不为它预留布局空间（`DesiredSize()` 恒为 `{0, 0}`），
 * 因此 `DuiHBox` / `DuiVBox` 这类会分配空间的容器需要在自身 `Layout` 里显式转发。
 * 生效期间绘制半透明遮罩、高亮当前步骤的目标元素，并在元素旁显示引导框；
 * 引导框含标题、正文、步骤计数与「跳过 / 上一步 / 下一步（完成）」按钮。
 *
 * 命中测试：生效期间独占可视区（遮罩挡住下层交互），仅引导框内的按钮可点击。
 *
 * 使用方式：
 * @code
 * guide.SetSteps({...});
 * guide.SetFinishHandler([](int, int) { / * 引导完成 * / });
 * parent->AddChild(std::move(guideControl)); // 作为最后一个子节点加入
 * guide.Show();
 * @endcode
 */
class DuiGuide final : public core::Control, public render::DuiRenderable
{
public:
    DuiGuide();
    ~DuiGuide() override;
    DuiGuide(const DuiGuide&) = delete;
    DuiGuide& operator=(const DuiGuide&) = delete;
    DuiGuide(DuiGuide&&) = delete;
    DuiGuide& operator=(DuiGuide&&) = delete;

    /**
     * 设置引导步骤。
     * @param steps 步骤序列；为空时 `Show()` 不显示任何内容
     */
    void SetSteps(std::vector<DuiGuideStep> steps);
    /** @return 当前步骤序列 */
    [[nodiscard]] const std::vector<DuiGuideStep>& Steps() const;
    /** @return 步骤总数 */
    [[nodiscard]] int StepCount() const;

    /**
     * 设置引导框类型。
     * @param mode 气泡或居中弹窗
     */
    void SetMode(DuiGuideMode mode);
    /** @return 当前引导框类型 */
    [[nodiscard]] DuiGuideMode Mode() const;

    /**
     * 设置是否绘制遮罩层。
     * @param show false 时只显示引导框与高亮框
     */
    void SetShowOverlay(bool show);
    /** @return 是否绘制遮罩层 */
    [[nodiscard]] bool ShowOverlay() const;
    /**
     * 设置步骤级默认高亮内边距（步骤未单独指定时生效）。
     * @param padding 像素；负数按 0 处理
     */
    void SetHighlightPadding(int padding);
    /** @return 默认高亮内边距 */
    [[nodiscard]] int HighlightPadding() const;
    /**
     * 设置引导框固定尺寸。
     * 说明：控件不注入文本测量器，不做内容自适应，正文超出高度会被裁剪。
     *       宽度若不足以容纳底部按钮（如最后一步同时出现跳过/上一步/完成），
     *       会按需要加宽，不会出现按钮溢出引导框。
     * @param size 宽度或高度非正时该方向回退到默认值
     */
    void SetCardSize(core::Size size);
    /** @return 引导框固定尺寸 */
    [[nodiscard]] core::Size CardSize() const;

    /**
     * 设置是否隐藏步骤计数。
     * @param hide true 时不显示「当前/总数」
     */
    void SetHideCounter(bool hide);
    /** @return 是否隐藏步骤计数 */
    [[nodiscard]] bool HideCounter() const;
    /**
     * 设置是否隐藏「上一步」按钮。
     * 说明：未隐藏时首步仍不显示，与常见引导交互一致。
     */
    void SetHidePrev(bool hide);
    /** @return 是否隐藏「上一步」按钮 */
    [[nodiscard]] bool HidePrev() const;
    /** 设置是否隐藏「跳过」按钮。 */
    void SetHideSkip(bool hide);
    /** @return 是否隐藏「跳过」按钮 */
    [[nodiscard]] bool HideSkip() const;

    /** 设置「下一步」按钮文案与样式。 */
    void SetNextButtonProps(DuiGuideButtonProps props);
    /** 设置「上一步」按钮文案与样式。 */
    void SetPrevButtonProps(DuiGuideButtonProps props);
    /** 设置「跳过」按钮文案与样式。 */
    void SetSkipButtonProps(DuiGuideButtonProps props);
    /** 设置最后一步「下一步」（即完成）按钮文案与样式。 */
    void SetFinishButtonProps(DuiGuideButtonProps props);

    /**
     * 直接切换到指定步骤（编程式，不触发 change 回调）。
     * @param current 目标步骤下标；越界忽略
     */
    void SetCurrent(int current);
    /** @return 当前步骤下标 */
    [[nodiscard]] int Current() const;
    /**
     * 设置 `Show()` 的起始步骤（非受控语义，不触发回调）。
     * @param current 起始下标；越界时被钳制到有效范围
     */
    void SetDefaultCurrent(int current);
    /** @return `Show()` 的起始步骤 */
    [[nodiscard]] int DefaultCurrent() const;

    /** 设置当前步骤变化回调（由按钮或 `Next`/`Prev` 引起）。 */
    void SetChangeHandler(std::function<void(int current, int total)> handler);
    /** 设置点击「下一步」回调；最后一步点击「完成」改触发 SetFinishHandler。 */
    void SetNextStepClickHandler(std::function<void(int next, int current, int total)> handler);
    /** 设置点击「上一步」回调。 */
    void SetPrevStepClickHandler(std::function<void(int prev, int current, int total)> handler);
    /** 设置点击「跳过」回调；回调后引导自动关闭。 */
    void SetSkipHandler(std::function<void(int current, int total)> handler);
    /** 设置完成回调；回调后引导自动关闭。 */
    void SetFinishHandler(std::function<void(int current, int total)> handler);

    /** 从 DefaultCurrent 开始显示引导；无步骤时不显示。 */
    void Show();
    /** 关闭引导。 */
    void Hide();
    /** @return 引导是否显示中 */
    [[nodiscard]] bool Active() const;
    /** 等价点击「下一步」：末步触发完成并关闭，否则前进一步。 */
    void Next();
    /** 等价点击「上一步」：首步无效果。 */
    void Prev();
    /** 等价点击「跳过」：触发跳过回调并关闭。 */
    void Skip();

    void Layout(core::Rect bounds) override;
    /** @return 覆盖层无固有尺寸，恒为 {0, 0}。 */
    [[nodiscard]] core::Size DesiredSize() const override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    /** 应用步骤下标并刷新引导框；notify 为 true 时触发 change 回调。 */
    void applyStep(int index, bool notify);
    /** 同步引导框文案、配色与按钮可见性到当前步骤。 */
    void syncCard();
    /** 按当前步骤与控件矩形重排引导框。 */
    void updateCardLayout();

    class Impl;
    std::unique_ptr<Impl> guide_;
};

} // namespace ysDui::controls::window
