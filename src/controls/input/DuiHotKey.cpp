#include "ysDui/controls/input/DuiHotKey.hpp"

#include <utility>

#include "ysDui/core/DuiTheme.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int DefaultWidth = 160;
constexpr int DefaultHeight = 25;

std::string NumberText(unsigned int value) {
    const std::string text = std::to_string(value);
    return {text.begin(), text.end()};
}

std::string KeyName(unsigned int key) {
    if (key >= '0' && key <= '9') return std::string(1, static_cast<char>(key));
    if (key >= 'A' && key <= 'Z') return std::string(1, static_cast<char>(key));
    if (key >= core::key::Function1 && key < core::key::Function1 + 24) {
        return "F" + NumberText(key - core::key::Function1 + 1);
    }
    switch (key) {
    case core::key::Backspace: return "Backspace";
    case core::key::Enter: return "Enter";
    case core::key::Space: return "Space";
    case core::key::Delete: return "Delete";
    default: return "Key " + NumberText(key);
    }
}

bool IsModifier(unsigned int key) {
    return key == core::key::Shift || key == core::key::Control || key == core::key::Alt || key == core::key::Meta;
}
}

class DuiHotKey::Impl {
public:
    unsigned int key{};
    unsigned int modifiers{};
    std::function<void(unsigned int, unsigned int)> changed;
    render::DuiTextStyle textStyle{{30, 30, 40, 255}, "Segoe UI, Arial", 9, false};
};

DuiHotKey::DuiHotKey() : hotKey_(std::make_unique<Impl>()) {}
DuiHotKey::~DuiHotKey() = default;
DuiHotKey::DuiHotKey(DuiHotKey&&) noexcept = default;
DuiHotKey& DuiHotKey::operator=(DuiHotKey&&) noexcept = default;
void DuiHotKey::SetHotKey(unsigned int key, unsigned int modifiers, bool notify) {
    if (hotKey_->key == key && hotKey_->modifiers == modifiers) return;
    hotKey_->key = key;
    hotKey_->modifiers = modifiers;
    if (notify && hotKey_->changed) hotKey_->changed(key, modifiers);
}
void DuiHotKey::Clear(bool notify) { SetHotKey(0, 0, notify); }
unsigned int DuiHotKey::Key() const { return hotKey_->key; }
unsigned int DuiHotKey::Modifiers() const { return hotKey_->modifiers; }
bool DuiHotKey::Empty() const { return hotKey_->key == 0; }
std::string DuiHotKey::FormatHotKey(unsigned int key, unsigned int modifiers) {
    if (key == 0) return {};
    std::string result;
    if (modifiers & core::modifier::Control) result += "Ctrl+";
    if (modifiers & core::modifier::Shift) result += "Shift+";
    if (modifiers & core::modifier::Alt) result += "Alt+";
    if (modifiers & core::modifier::Meta) result += "Meta+";
    return result + KeyName(key);
}
std::string DuiHotKey::DisplayText() const { return FormatHotKey(Key(), Modifiers()); }
void DuiHotKey::SetValueChangedHandler(std::function<void(unsigned int, unsigned int)> handler) { hotKey_->changed = std::move(handler); }
void DuiHotKey::Layout(core::Rect bounds) { SetBounds(bounds); }
core::Size DuiHotKey::DesiredSize() const { return {DefaultWidth, DefaultHeight}; }
bool DuiHotKey::OnEvent(const core::Event& event) {
    if (!Enabled()) return false;
    if (event.type == core::EventType::PointerDown) return Bounds().Contains(event.position);
    if (event.type != core::EventType::KeyDown || !Focused()) return false;
    if (event.key == core::key::Tab || event.key == core::key::Escape) return false;
    if (event.key == core::key::Backspace || event.key == core::key::Delete) { Clear(true); return true; }
    if (IsModifier(event.key)) return true;
    SetHotKey(event.key, event.modifiers, true);
    return true;
}
void DuiHotKey::Paint(render::Canvas& canvas, core::Rect dirty) const {
    const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    const core::Color border = Focused() ? Theme().Get(core::ThemeSlot::BrandPrimary)
                                         : Theme().Get(core::ThemeSlot::FieldBorder);
    canvas.FillRect(bounds, Enabled() ? Theme().Get(core::ThemeSlot::SurfaceBackground)
                                      : Theme().Get(core::ThemeSlot::FieldDisabledBackground));
    canvas.StrokeRoundedRect(bounds, 0, border, 1.0f);
    const std::string text = DisplayText();
    auto style = hotKey_->textStyle;
    style.color = !Enabled() ? Theme().Get(core::ThemeSlot::FieldDisabledText)
                             : text.empty() ? Theme().Get(core::ThemeSlot::FieldPlaceholder)
                                            : Theme().Get(core::ThemeSlot::ControlText);
    canvas.DrawText(text.empty() ? (Focused() ? "Press shortcut" : "(none)") : text,
                    {bounds.left + 8, bounds.top, bounds.right - 8, bounds.bottom}, style,
                    render::DuiTextAlignment::Start, false);
}

} // namespace ysDui::controls::input
