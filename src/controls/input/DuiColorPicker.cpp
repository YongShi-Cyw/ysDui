#include "ysDui/controls/input/DuiColorPicker.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiFocusVisual.hpp"
#include "ysDui/render/DuiPopupSurface.hpp"

namespace ysDui::controls::input {
namespace {
constexpr int DefaultHeight = 25;
constexpr int SwatchWidth = 22;
constexpr int ArrowWidth = 22;
constexpr int GridColumns = 8;
constexpr int GridRows = 5;
constexpr int CellSize = 18;
constexpr int Padding = 6;
constexpr int MoreHeight = 24;

constexpr std::array Presets{
    core::Color{0, 0, 0}, core::Color{64, 64, 64}, core::Color{128, 128, 128}, core::Color{192, 192, 192},
    core::Color{255, 255, 255}, core::Color{255, 0, 0}, core::Color{255, 128, 0}, core::Color{255, 255, 0},
    core::Color{0, 255, 0}, core::Color{0, 255, 255}, core::Color{0, 128, 255}, core::Color{0, 0, 255},
    core::Color{128, 0, 255}, core::Color{255, 0, 255}, core::Color{128, 0, 0}, core::Color{0, 128, 0},
    core::Color{0, 0, 128}, core::Color{128, 128, 0}, core::Color{0, 128, 128}, core::Color{128, 0, 128},
    core::Color{45, 108, 223}, core::Color{220, 53, 69}, core::Color{40, 167, 69}, core::Color{255, 193, 7},
    core::Color{23, 162, 184}, core::Color{108, 117, 125}, core::Color{52, 58, 64}, core::Color{248, 249, 250},
    core::Color{233, 236, 239}, core::Color{222, 226, 230}, core::Color{206, 212, 218}, core::Color{173, 181, 189},
    core::Color{134, 142, 150}, core::Color{73, 80, 87}, core::Color{33, 37, 41}, core::Color{255, 99, 132},
    core::Color{54, 162, 235}, core::Color{255, 205, 86}, core::Color{75, 192, 192}, core::Color{153, 102, 255}};

constexpr int GridWidth = GridColumns * CellSize + Padding * 2;
constexpr int GridHeight = GridRows * CellSize + Padding * 2;

int HexValue(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

char HexDigit(unsigned char value)
{
    return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
}

class ColorPopup final : public core::Control, public render::DuiRenderable {
public:
    ColorPopup(std::function<void(core::Color)> picked, std::function<void()> more)
        : picked_(std::move(picked)), more_(std::move(more)) {}

    bool OnEvent(const core::Event& event) override
    {
        if (event.type != core::EventType::PointerDown || !Bounds().Contains(event.position)) return false;
        const int x = event.position.x - Bounds().left - Padding;
        const int y = event.position.y - Bounds().top - Padding;
        if (y >= GridRows * CellSize) {
            more_();
            return true;
        }
        const int column = x / CellSize;
        const int row = y / CellSize;
        if (x < 0 || y < 0 || column >= GridColumns || row >= GridRows) return true;
        picked_(Presets[static_cast<std::size_t>(row * GridColumns + column)]);
        return true;
    }

    void Paint(render::Canvas& canvas, core::Rect dirty) const override
    {
        const auto bounds = core::Rect::Intersect(Bounds(), dirty);
        if (bounds.Empty()) return;
        canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::SurfaceBackground));
        for (int index = 0; index < static_cast<int>(Presets.size()); ++index) {
            const int row = index / GridColumns;
            const int column = index % GridColumns;
            const core::Rect cell{Bounds().left + Padding + column * CellSize, Bounds().top + Padding + row * CellSize,
                                  Bounds().left + Padding + (column + 1) * CellSize, Bounds().top + Padding + (row + 1) * CellSize};
            canvas.FillRect(cell, Presets[static_cast<std::size_t>(index)]);
            canvas.StrokeRoundedRect(cell, 0, Theme().Get(core::ThemeSlot::ColorGridBorder), 1.0F);
        }
        canvas.DrawText("More\xE2\x80\xA6",
                        {Bounds().left + Padding, Bounds().top + GridHeight,
                         Bounds().right - Padding, Bounds().bottom},
                        {Theme().Get(core::ThemeSlot::TextLink), style_.family, style_.pointSize,
                         style_.bold, style_.underline}, render::DuiTextAlignment::Center, false);
        render::PaintPopupBorder(canvas, Bounds(), Theme());
    }

private:
    std::function<void(core::Color)> picked_;
    std::function<void()> more_;
    render::DuiTextStyle style_{{45, 108, 223, 255}, {}, 9, false, false};
};
} // namespace

class DuiColorPicker::Impl {
public:
    core::Color color{45, 108, 223, 255};
    ui::IPopupHost* popup{};
    std::shared_ptr<int> popupLifetime;
    ui::DuiColorChooser* chooser{};
    std::function<void(core::Color)> changed;
    render::DuiTextStyle style;
    bool popupOpen{};
};

DuiColorPicker::DuiColorPicker() : colorPicker_(std::make_unique<Impl>()) {}
DuiColorPicker::~DuiColorPicker()
{
    colorPicker_->popupLifetime.reset();
    colorPicker_->popup = nullptr;
}

void DuiColorPicker::SetColor(core::Color color, bool notify)
{
    if (colorPicker_->color == color) return;
    colorPicker_->color = color;
    if (notify && colorPicker_->changed) colorPicker_->changed(color);
}

core::Color DuiColorPicker::Color() const { return colorPicker_->color; }
void DuiColorPicker::SetColorChangedHandler(std::function<void(core::Color)> handler) { colorPicker_->changed = std::move(handler); }

std::string DuiColorPicker::FormatColorHex(core::Color color)
{
    return {'#', HexDigit(static_cast<unsigned char>(color.red >> 4)), HexDigit(static_cast<unsigned char>(color.red & 0x0F)),
            HexDigit(static_cast<unsigned char>(color.green >> 4)), HexDigit(static_cast<unsigned char>(color.green & 0x0F)),
            HexDigit(static_cast<unsigned char>(color.blue >> 4)), HexDigit(static_cast<unsigned char>(color.blue & 0x0F))};
}

core::Color DuiColorPicker::ParseColorHex(std::string_view text)
{
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6) return {};
    std::array<int, 6> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = HexValue(text[index]);
        if (values[index] < 0) return {};
    }
    return {static_cast<unsigned char>((values[0] << 4) | values[1]), static_cast<unsigned char>((values[2] << 4) | values[3]),
            static_cast<unsigned char>((values[4] << 4) | values[5]), 255};
}

void DuiColorPicker::SetPopupHost(ui::IPopupHost* popup)
{
    if (colorPicker_->popup == popup) return;
    colorPicker_->popupLifetime.reset();
    if (colorPicker_->popup) {
        colorPicker_->popup->SetDismissedHandler({});
        if (colorPicker_->popupOpen) colorPicker_->popup->RequestHide();
    }
    colorPicker_->popupOpen = false;
    colorPicker_->popup = popup;
    if (popup) {
        colorPicker_->popupLifetime = std::make_shared<int>();
        const std::weak_ptr<int> lifetime = colorPicker_->popupLifetime;
        popup->SetDismissedHandler([lifetime, state = colorPicker_.get()] {
            if (lifetime.lock()) state->popupOpen = false;
        });
    }
}

void DuiColorPicker::SetColorChooser(ui::DuiColorChooser* chooser) { colorPicker_->chooser = chooser; }
bool DuiColorPicker::PopupOpen() const { return colorPicker_->popupOpen; }

void DuiColorPicker::OpenPopup()
{
    if (!colorPicker_->popup || colorPicker_->popupOpen) return;
    Impl* state = colorPicker_.get();
    const std::weak_ptr<int> lifetime = colorPicker_->popupLifetime;
    auto popup = std::make_unique<ColorPopup>([lifetime, state](core::Color color) {
        if (!lifetime.lock()) return;
        state->color = color;
        if (state->changed) state->changed(color);
        if (state->popup) state->popup->RequestHide();
        state->popupOpen = false;
    }, [lifetime, state] {
        if (!lifetime.lock()) return;
        if (state->chooser) {
            const auto result = state->chooser->Choose(state->color);
            if (result.accepted) {
                state->color = result.color;
                if (state->changed) state->changed(result.color);
            }
        }
        if (state->popup) state->popup->RequestHide();
        state->popupOpen = false;
    });
    ColorPopup* popupContent = popup.get();
    popupContent->SetTheme(&Theme());
    ui::DuiPopupOptions options;
    options.anchor = Bounds();
    options.size = {GridWidth, GridHeight + MoreHeight};
    colorPicker_->popupOpen = colorPicker_->popup->Show(options, std::move(popup),
        [popupContent](core::Rect bounds) { popupContent->SetBounds(bounds); },
        [popupContent](render::Canvas& canvas, core::Rect dirty) { popupContent->Paint(canvas, dirty); });
}

void DuiColorPicker::ClosePopup()
{
    if (colorPicker_->popup && colorPicker_->popupOpen) colorPicker_->popup->RequestHide();
    colorPicker_->popupOpen = false;
}

core::Rect DuiColorPicker::SwatchRect() const
{
    const auto bounds = Bounds();
    return {bounds.left + 4, bounds.top + 3, bounds.left + 4 + SwatchWidth, bounds.bottom - 3};
}

core::Rect DuiColorPicker::ArrowRect() const
{
    const auto bounds = Bounds();
    return {bounds.right - ArrowWidth, bounds.top, bounds.right, bounds.bottom};
}

core::Size DuiColorPicker::DesiredSize() const { return {140, DefaultHeight}; }
void DuiColorPicker::Layout(core::Rect bounds) { SetBounds(bounds); }

bool DuiColorPicker::OnEvent(const core::Event& event)
{
    if (!Enabled()) return false;
    const bool clicked = event.type == core::EventType::PointerDown && Bounds().Contains(event.position);
    const bool activated = event.type == core::EventType::KeyDown &&
                           (event.key == core::key::Enter || event.key == core::key::Space);
    if (clicked || activated) {
        if (PopupOpen()) ClosePopup(); else OpenPopup();
        return true;
    }
    return false;
}

void DuiColorPicker::Paint(render::Canvas& canvas, core::Rect dirty) const
{
    const auto bounds = core::Rect::Intersect(Bounds(), dirty);
    if (!EffectivelyVisible() || bounds.Empty()) return;
    canvas.FillRect(bounds, Theme().Get(core::ThemeSlot::SurfaceBackground));
    canvas.StrokeRoundedRect(bounds, 0, Theme().Get(core::ThemeSlot::FieldBorder), 1.0F);
    canvas.FillRect(SwatchRect(), Color());
    const auto arrow = ArrowRect();
    render::DuiPath path;
    const int centerX = (arrow.left + arrow.right) / 2;
    const int centerY = (arrow.top + arrow.bottom) / 2;
    path.MoveTo({centerX - 4, centerY - 2});
    path.LineTo({centerX + 4, centerY - 2});
    path.LineTo({centerX, centerY + 3});
    path.Close();
    canvas.FillPath(path, Theme().Get(core::ThemeSlot::ColorPickerArrow));
    canvas.DrawText(FormatColorHex(Color()), {SwatchRect().right + 6, bounds.top, arrow.left - 4, bounds.bottom},
                    {Theme().Get(core::ThemeSlot::ControlText), colorPicker_->style.family,
                     colorPicker_->style.pointSize, colorPicker_->style.bold, colorPicker_->style.underline},
                    render::DuiTextAlignment::Start, false);
    if (Focused() && Enabled())
        render::DrawThemedFocusRing(canvas, bounds, Theme());
}

} // namespace ysDui::controls::input
