/**
 * 文件名：DuiAutoSuggestBox.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：输入即弹出建议列表；可静态列表或动态提供器。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace ysDui::controls::input {

/**
 * 搜索建议框：可编辑组合框 + 输入时刷新建议并打开弹出列表。
 */
class DuiAutoSuggestBox final : public core::Control, public render::DuiRenderable {
public:
    DuiAutoSuggestBox();
    ~DuiAutoSuggestBox() override;
    DuiAutoSuggestBox(const DuiAutoSuggestBox&) = delete;
    DuiAutoSuggestBox& operator=(const DuiAutoSuggestBox&) = delete;
    DuiAutoSuggestBox(DuiAutoSuggestBox&&) = delete;
    DuiAutoSuggestBox& operator=(DuiAutoSuggestBox&&) = delete;

    /**
     * 用静态建议替换当前列表。
     * @param items UTF-8 建议
     */
    void SetSuggestions(std::vector<std::string> items);
    /**
     * 设置动态建议提供器；非空时每次文本变化都会调用并替换列表。
     * @param provider 入参为当前文本，返回建议；可为空
     */
    void SetSuggestionProvider(std::function<std::vector<std::string>(std::string_view)> provider);
    void SetTextInput(ui::DuiTextInput* input);
    void SetPopupHost(ui::IPopupHost* popup);
    void SetText(std::string text, bool notify = false);
    [[nodiscard]] std::string Text() const;
    [[nodiscard]] int SuggestionCount() const;
    void SetSelectionChangedHandler(std::function<void(int)> handler);
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    void refreshFromProvider();
    class Impl;
    std::unique_ptr<Impl> box_;
};

} // namespace ysDui::controls::input
