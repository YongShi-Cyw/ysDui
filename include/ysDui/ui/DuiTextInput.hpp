/**
 * 文件名：DuiTextInput.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的 UTF-8 文本输入能力。
 */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"

namespace ysDui::ui {

struct DuiTextInputOptions final
{
    bool multiline{};
    bool wordWrap{};
    bool password{};
    bool readOnly{};
    int maxLength{};
    /**
     * 多行输入下按 Enter 触发提交，Shift+Enter 仍插入换行。
     * 单行输入无论该值如何都由 Enter 提交（既有行为不变）。默认为 false，即多行输入的 Enter 用于换行。
     */
    bool submitOnEnter{};
};

/** 普通文本输入中的 UTF-8 字节选区。 */
struct DuiTextSelection final
{
    std::size_t start{};
    std::size_t end{};
    constexpr bool operator==(const DuiTextSelection&) const = default;
};

class DuiTextInput
{
public:
    virtual ~DuiTextInput() = default;
    virtual void SetBounds(core::Rect bounds) = 0;
    [[nodiscard]] virtual core::Rect Bounds() const = 0;

    /**
     * 设置输入会话是否可用；平台原生输入代理不得用该状态承担控件可见绘制。
     * @param visible 为 true 时允许代理获取焦点，为 false 时结束当前输入焦点。
     */
    virtual void SetVisible(bool visible) = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void SetBorderVisible(bool visible) = 0;
    virtual void SetOptions(const DuiTextInputOptions&) = 0;
    virtual void SetPlaceholder(std::string text) = 0;
    virtual void SetText(std::string text) = 0;
    [[nodiscard]] virtual std::string Text() const = 0;

    /**
     * 获取 UTF-8 文本中的光标字节位置，供平台无关控件自绘光标。
     * @return 光标前方 UTF-8 文本的字节数；不支持选择查询的实现默认返回文本末尾。
     */
    [[nodiscard]] virtual std::size_t CaretPosition() const { return Text().size(); }

    /** @return 当前自绘插入光标是否处于可见相位；不支持闪烁的实现默认始终可见。 */
    [[nodiscard]] virtual bool CaretVisible() const { return true; }

    /** @return 按 UTF-8 字节位置表示的规范化选区。 */
    [[nodiscard]] virtual DuiTextSelection TextSelection() const
    {
        const std::size_t caret = CaretPosition();
        return {caret, caret};
    }

    virtual void Focus() = 0;

    /**
     * 获取焦点并将光标定位到控件坐标；不支持命中定位的实现退化为普通 Focus()。
     * @param position 平台无关的控件坐标。
     */
    virtual void FocusAt(core::Point position) { (void)position; Focus(); }

    /** 获取焦点，并以控件坐标开始鼠标选区。 */
    virtual void BeginSelection(core::Point position) { FocusAt(position); }
    /** 将鼠标选区扩展到控件坐标。 */
    virtual void UpdateSelection(core::Point position) { (void)position; }
    /** 结束当前鼠标选区。 */
    virtual void EndSelection() {}
    /**
     * 双击选词：选中控件坐标所在的单词（按平台自身选词规则）。
     * 用途：原生输入会话需处理鼠标按下/拖动之外的选词语义时由平台实现。
     * @param position 平台无关的控件坐标。
     */
    virtual void SelectWordAt(core::Point position) { (void)position; }

    virtual void SetChangedHandler(std::function<void()> handler) = 0;
    virtual void SetFocusLostHandler(std::function<void()> handler) = 0;

    /**
     * 设置单行输入的提交回调；不支持提交语义的实现可保留默认空实现。
     * @param handler 用户确认当前输入时调用的回调。
     */
    virtual void SetSubmitHandler(std::function<void()> handler) { (void)handler; }

    /**
     * 设置取消回调（如 Esc）；未设置时平台可保留默认按键行为。
     * @param handler 用户取消当前输入时调用的回调。
     */
    virtual void SetCancelHandler(std::function<void()> handler) { (void)handler; }

    /**
     * 获取输入对象的生命周期标记，供持有非拥有引用的控件避免在析构后访问对象。
     * @return 输入对象存活期间有效的弱引用标记。
     */
    [[nodiscard]] std::weak_ptr<void> LifetimeToken() const noexcept { return lifetime_; }

private:
    std::shared_ptr<void> lifetime_{std::make_shared<int>(0)};
};

} // namespace ysDui::ui
