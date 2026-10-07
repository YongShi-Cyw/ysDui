/**
 * 文件名：DuiAutoSuggestBox.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-06
 * 用途：组合框封装：输入变化时刷新建议并打开弹出列表。
 */
#include "ysDui/controls/input/DuiAutoSuggestBox.hpp"

#include <utility>

#include "ysDui/controls/input/DuiComboBox.hpp"

namespace ysDui::controls::input {

class DuiAutoSuggestBox::Impl {
public:
    DuiComboBox* combo{};
    std::function<std::vector<std::string>(std::string_view)> provider;
    std::function<void(int)> changed;
};

DuiAutoSuggestBox::DuiAutoSuggestBox() : box_(std::make_unique<Impl>())
{
    auto combo = std::make_unique<DuiComboBox>();
    box_->combo = combo.get();
    box_->combo->SetEditable(true);
    box_->combo->SetIncrementalSearch(true);
    box_->combo->SetSubstringSearch(true);
    box_->combo->SetSelectionChangedHandler([this](int index)
    {
        if (box_->changed)
            box_->changed(index);
    });
    AddChild(std::move(combo));
}

DuiAutoSuggestBox::~DuiAutoSuggestBox() = default;

void DuiAutoSuggestBox::refreshFromProvider()
{
    if (!box_->provider)
        return;
    const std::string text = box_->combo->Text();
    const auto items = box_->provider(text);
    while (box_->combo->Count() > 0)
        box_->combo->RemoveItem(0);
    for (const auto& item : items)
        box_->combo->AddItem(item);
}

void DuiAutoSuggestBox::SetSuggestions(std::vector<std::string> items)
{
    box_->combo->ClearItems();
    for (auto& item : items)
        box_->combo->AddItem(std::move(item));
}

void DuiAutoSuggestBox::SetSuggestionProvider(
    std::function<std::vector<std::string>(std::string_view)> provider)
{
    box_->provider = std::move(provider);
}

void DuiAutoSuggestBox::SetTextInput(ui::DuiTextInput* input)
{
    box_->combo->SetTextInput(input);
    if (input == nullptr)
        return;
    input->SetChangedHandler([this]
    {
        refreshFromProvider();
        box_->combo->ClosePopup();
        if (!box_->combo->Text().empty())
            box_->combo->OpenPopup();
    });
}

void DuiAutoSuggestBox::SetPopupHost(ui::IPopupHost* popup) { box_->combo->SetPopupHost(popup); }
void DuiAutoSuggestBox::SetText(std::string text, bool notify)
{
    box_->combo->SetText(std::move(text), notify);
    refreshFromProvider();
}
std::string DuiAutoSuggestBox::Text() const { return box_->combo->Text(); }
int DuiAutoSuggestBox::SuggestionCount() const { return box_->combo->Count(); }
void DuiAutoSuggestBox::SetSelectionChangedHandler(std::function<void(int)> handler)
{
    box_->changed = std::move(handler);
}
void DuiAutoSuggestBox::Layout(core::Rect bounds)
{
    SetBounds(bounds);
    box_->combo->Layout(bounds);
}
bool DuiAutoSuggestBox::OnEvent(const core::Event& event) { return box_->combo->OnEvent(event); }
void DuiAutoSuggestBox::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    box_->combo->Paint(canvas, dirty);
}

} // namespace ysDui::controls::input
