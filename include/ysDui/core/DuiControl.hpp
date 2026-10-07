#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiAccessibility.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiVisualState.hpp"

namespace ysDui::core {

class DuiTheme;

/** 控件请求的平台无关鼠标光标。 */
enum class DuiPointerCursor {
    /** 标准箭头。 */
    Arrow,
    /** 可点击区域的手形光标。 */
    Hand,
    /** 水平方向调整大小。 */
    ResizeHorizontal,
    /** 垂直方向调整大小。 */
    ResizeVertical,
    /** 文本输入区域的 I-beam 光标。 */
    IBeam,
    /** 四向拖拽移动光标。 */
    Move,
    /** 右下角对角缩放光标（左上—右下方向）。 */
    ResizeDownRight,
};

class Control {
public:
    Control();
    virtual ~Control();
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;
    Control(Control&&) noexcept;
    Control& operator=(Control&&) noexcept;

    void SetBounds(Rect bounds);
    [[nodiscard]] Rect Bounds() const;
    /**
     * 按给定外框排列自身（及容器子树）。
     * 默认实现只写入 Bounds；布局容器应重写并把子控件的最终矩形交给 `Layout` 而不是 `SetBounds`，
     * 以便嵌套容器自动向下重排。
     */
    virtual void Layout(Rect bounds);
    /**
     * 标记从本节点到根需要重排。
     * 宿主在下一帧 `PrepareFrame` / `Dispatch` 时对根调用 `PerformLayout()`。
     * 布局过程中（`Layout` 内部）调用无效，避免循环。
     */
    void InvalidateLayout();
    /** @return 本节点是否仍有未应用的布局失效。 */
    [[nodiscard]] bool LayoutDirty() const;
    /**
     * 若本节点布局已失效，则以当前 Bounds 调用 `Layout`。
     * 已在布局中或未失效时为空操作。
     */
    void PerformLayout();
    /**
     * 控件在无外部约束时的首选尺寸（DIP）。
     * 这是控件自报尺寸的唯一协议入口：容器与宿主可经 `Control&` 多态查询，
     * 无需知道具体控件类型。默认返回 `{0, 0}`，表示"无偏好、由容器决定"。
     * 具体控件应重写并声明 `override`。
     */
    [[nodiscard]] virtual Size DesiredSize() const;
    void SetVisible(bool visible);
    [[nodiscard]] bool Visible() const;
    [[nodiscard]] bool EffectivelyVisible() const;
    void SetEnabled(bool enabled);
    /**
     * @return 是否可交互。
     * 沿父链求值：任一祖先被禁用则自身也视为禁用（与 `EffectivelyVisible()` 同构）。
     * 容器因此可以整体停用子树——后代无需感知，会自动取禁用配色并停止响应输入。
     */
    [[nodiscard]] bool Enabled() const;
    void SetFocused(bool focused);
    [[nodiscard]] bool Focused() const;
    void SetHovered(bool hovered);
    [[nodiscard]] bool Hovered() const;
    void SetCaptured(bool captured);
    [[nodiscard]] bool Captured() const;
    [[nodiscard]] VisualState GetVisualState() const;
    void SetName(std::string name);
    [[nodiscard]] const std::string& Name() const;
    /** 设置平台无关的无障碍名称覆盖；空字符串恢复控件默认名称。 */
    void SetAccessibilityName(std::string name);
    /** 设置平台无关的无障碍说明覆盖；空字符串恢复控件默认说明。 */
    void SetAccessibilityDescription(std::string description);
    /** 设置供平台无障碍后端映射的稳定字符串标识。 */
    void SetAccessibilityIdentifier(std::string identifier);
    /** 返回已经合并控件默认语义和显式覆盖的无障碍数据。 */
    [[nodiscard]] DuiAccessibilityData Accessibility() const;
    /**
     * 执行平台无关的无障碍动作。
     * @param action 要执行的动作。
     * @param value SetValue 动作使用的 UTF-8 文本，其他动作忽略。
     * @return 动作已执行时返回 true；不支持或当前状态不允许时返回 false。
     */
    virtual bool PerformAccessibilityAction(DuiAccessibilityAction action,
                                            std::string_view value = {});
    [[nodiscard]] Control* Parent() const;

    /** 设置当前控件及其后代使用的显式主题；传入 nullptr 时恢复继承主题。 */
    void SetTheme(const DuiTheme* theme);
    /** @return 按“控件显式覆盖 > 父级或 Host > 内置默认”解析后的主题。 */
    [[nodiscard]] const DuiTheme& Theme() const;

    void AddChild(std::unique_ptr<Control> child);
    [[nodiscard]] std::unique_ptr<Control> RemoveChild(Control* child);
    [[nodiscard]] std::vector<std::unique_ptr<Control>>& Children();
    [[nodiscard]] const std::vector<std::unique_ptr<Control>>& Children() const;
    [[nodiscard]] virtual Control* HitTest(Point point);
    /** 设置平台无关的鼠标光标语义。 */
    void SetPointerCursor(DuiPointerCursor cursor);
    /** 返回平台无关的鼠标光标语义。 */
    [[nodiscard]] DuiPointerCursor PointerCursor() const;

    virtual bool OnEvent(const Event& event);

protected:
    /** 由具体控件提供角色、默认名称、值和键盘焦点能力。 */
    [[nodiscard]] virtual DuiAccessibilityData CreateAccessibilityData() const;

private:
    friend class Host;
    void SetInheritedTheme(const DuiTheme* theme);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::core
