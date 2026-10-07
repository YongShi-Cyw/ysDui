#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiTextInput.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiEditHost final : public core::Control, public render::DuiRenderable {
public:
    DuiEditHost();
    ~DuiEditHost() override;
    DuiEditHost(const DuiEditHost&) = delete;
    DuiEditHost& operator=(const DuiEditHost&) = delete;
    void SetTextInput(ui::DuiTextInput* input);
    void SetText(std::string text, bool notify = false);
    [[nodiscard]] const std::string& Text() const;
    void SetPlaceholder(std::string text);
    void SetOptions(ui::DuiTextInputOptions options);
    [[nodiscard]] ui::DuiTextInputOptions Options() const;
    void SetTextChangedHandler(std::function<void(std::string_view)> handler);
    /**
     * 设置"用户确认输入"的回调。
     * 触发条件与 `ui::DuiTextInputOptions::submitOnEnter` 一致：
     * 单行输入按 Enter 即触发；多行输入需开启该选项，且 Shift+Enter 仍为换行（不触发）。
     * 已绑定原生输入时由平台实现触发；未绑定原生输入（Canvas 回退）时由本控件在 Enter 键上触发。
     * @param handler 确认回调
     */
    void SetSubmitHandler(std::function<void()> handler);
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    void SetVisible(bool visible);
    void SetEnabled(bool enabled);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override;
    bool PerformAccessibilityAction(core::DuiAccessibilityAction action,
                                    std::string_view value) override;

private:
    class Impl;
    std::unique_ptr<Impl> editHost_;
};

} // namespace ysDui::controls::input
