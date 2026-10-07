#include "ysDui/controls/window/DuiDialog.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "DuiDialogSvg.hpp"
#include "ysDui/core/DuiEvent.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/render/DuiCaptionGlyph.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiPaintChildren.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiSvg.hpp"
#include "ysDui/render/DuiText.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"

namespace ysDui::controls::window {
namespace {
constexpr int TitleBarHeight = 24;
constexpr int DialogPadding = 16;
constexpr int DialogButtonWidth = 54;
constexpr int DialogButtonHeight = 25;
constexpr int DialogButtonGap = 8;
constexpr int DialogButtonHorizontalPadding = 16;
constexpr int DialogButtonBottom = 12;
constexpr int DialogPanelMargin = 12;
constexpr int DialogPanelPadding = 10;
constexpr int DialogPanelGap = 8;
struct ParsedButtonText final
{
    std::string display;
    std::size_t mnemonicOffset{std::string::npos};
    std::size_t mnemonicLength{};
};

std::size_t Utf8CharacterLength(std::string_view text, std::size_t offset)
{
    const auto lead = static_cast<unsigned char>(text[offset]);
    const std::size_t length = lead < 0x80U ? 1U : lead < 0xE0U ? 2U : lead < 0xF0U ? 3U : 4U;
    return (std::min)(length, text.size() - offset);
}

ParsedButtonText ParseButtonText(std::string_view text)
{
    ParsedButtonText result;
    result.display.reserve(text.size());
    for (std::size_t index{}; index < text.size();)
    {
        if (text[index] != '&')
        {
            result.display.push_back(text[index++]);
            continue;
        }
        if (index + 1 >= text.size())
            break;
        if (text[index + 1] == '&')
        {
            result.display.push_back('&');
            index += 2;
            continue;
        }
        const std::size_t length = Utf8CharacterLength(text, index + 1);
        if (result.mnemonicOffset == std::string::npos)
        {
            result.mnemonicOffset = result.display.size();
            result.mnemonicLength = length;
        }
        result.display.append(text.substr(index + 1, length));
        index += length + 1;
    }
    return result;
}

std::shared_ptr<const render::DuiImage> CreateCaptionIcon(std::string_view svg, core::Color color)
{
    auto buffer = render::RasterizeSvg(svg, {16, 16});
    if (!buffer)
        return {};
    std::vector<unsigned char> pixels(buffer->Bytes().begin(), buffer->Bytes().end());
    for (std::size_t offset{}; offset < pixels.size(); offset += 4)
    {
        const unsigned char alpha = pixels[offset + 3];
        pixels[offset] = static_cast<unsigned char>((static_cast<unsigned int>(color.blue) * alpha + 127U) / 255U);
        pixels[offset + 1] = static_cast<unsigned char>((static_cast<unsigned int>(color.green) * alpha + 127U) / 255U);
        pixels[offset + 2] = static_cast<unsigned char>((static_cast<unsigned int>(color.red) * alpha + 127U) / 255U);
    }
    return render::DuiImage::CreateBgra8Premultiplied(buffer->Size(), std::move(pixels));
}

struct CaptionIconImages final
{
    std::shared_ptr<const render::DuiImage> normal;
    std::shared_ptr<const render::DuiImage> hover;
};

CaptionIconImages CreateCaptionIcons(std::string_view svg, core::Color normal, core::Color hover)
{
    return {CreateCaptionIcon(svg, normal), CreateCaptionIcon(svg, hover)};
}

class DialogButton final : public core::Control, public render::DuiRenderable
{
public:
    DialogButton(std::string text, bool primary, std::function<void()> clicked)
        : text_(ParseButtonText(text)), clicked_(std::move(clicked)), primary_(primary)
    {
        SetPointerCursor(core::DuiPointerCursor::Hand);
    }

    bool OnEvent(const core::Event& event) override
    {
        if (!Enabled())
            return false;
        const bool contains = Bounds().Contains(event.position);
        if (event.type == core::EventType::PointerMove)
        {
            SetHovered(contains);
            return Captured();
        }
        if (event.type == core::EventType::PointerDown && contains)
        {
            SetHovered(true);
            SetCaptured(true);
            return true;
        }
        if (event.type == core::EventType::PointerCancel && Captured())
        {
            SetCaptured(false);
            SetHovered(false);
            return true;
        }
        if (event.type == core::EventType::PointerUp && Captured())
        {
            SetCaptured(false);
            SetHovered(contains);
            if (contains && clicked_)
                clicked_();
            return true;
        }
        if (event.type == core::EventType::KeyDown
            && (event.key == core::key::Enter || event.key == core::key::Space))
        {
            if (clicked_)
                clicked_();
            return true;
        }
        return false;
    }

    void Paint(render::Canvas& canvas, core::Rect dirty) const override
    {
        const core::Rect bounds = core::Rect::Intersect(Bounds(), dirty);
        if (!EffectivelyVisible() || bounds.Empty())
            return;
        const core::DuiTheme& theme = Theme();
        const core::Color accent = theme.Get(core::ThemeSlot::DialogAccent);
        core::Color fill = primary_ ? accent : theme.Get(core::ThemeSlot::DialogPanelBackground);
        core::Color border = accent;
        core::Color text = primary_ ? theme.Get(core::ThemeSlot::DialogTitleText) : accent;
        if (Hovered() && !primary_)
            fill = theme.Get(core::ThemeSlot::DialogButtonHover);
        if (Hovered() && primary_)
            fill = theme.Get(core::ThemeSlot::DialogPrimaryHover);
        if (Captured())
        {
            fill = theme.Get(core::ThemeSlot::DialogPrimaryPressed);
            // 按下时底色为实心主色，非默认按钮的强调色文字会与之同色难辨，统一改白字
            text = theme.Get(core::ThemeSlot::DialogTitleText);
        }
        if (!Enabled())
        {
            fill = theme.Get(core::ThemeSlot::DialogDisabledBackground);
            border = theme.Get(core::ThemeSlot::DialogDisabledBorder);
            text = theme.Get(core::ThemeSlot::DialogDisabledText);
        }
        canvas.FillRect(bounds, fill);
        canvas.StrokeRoundedRect(bounds, 0, border, 1.0F);
        const render::DuiTextStyle style{text, "Segoe UI, Arial, SimSun", 9, false};
        canvas.DrawText(text_.display, bounds, style,
                        render::DuiTextAlignment::Center, false);
        PaintMnemonicUnderline(canvas, bounds, style);
        // 焦点线框默认关闭，由主题开关控制
        if (Focused() && Enabled() && theme.FocusRingVisible())
        {
            const core::Rect focus{bounds.left + 3, bounds.top + 3, bounds.right - 3, bounds.bottom - 3};
            canvas.StrokeRoundedRect(focus, 0, primary_ ? theme.Get(core::ThemeSlot::DialogTitleText)
                                                       : accent, 1.0F);
        }
    }

    [[nodiscard]] int DesiredWidth(render::DuiTextMeasurer& measurer) const
    {
        const render::DuiTextStyle style{Theme().Get(core::ThemeSlot::DialogText),
                                         "Segoe UI, Arial, SimSun", 9, false};
        const int textWidth = measurer.MeasureText(text_.display, style, {}).size.width;
        return (std::max)(DialogButtonWidth, textWidth + DialogButtonHorizontalPadding);
    }

protected:
    [[nodiscard]] core::DuiAccessibilityData CreateAccessibilityData() const override
    {
        return {core::DuiAccessibilityRole::Button, text_.display, {}, {}, true, {}};
    }

private:
    void PaintMnemonicUnderline(render::Canvas& canvas, core::Rect bounds,
                                const render::DuiTextStyle& style) const
    {
        if (text_.mnemonicOffset == std::string::npos)
            return;
        const auto full = canvas.MeasureText(text_.display, style, {});
        const auto prefix = canvas.MeasureText(
            std::string_view(text_.display).substr(0, text_.mnemonicOffset), style, {});
        const auto mnemonic = canvas.MeasureText(
            std::string_view(text_.display).substr(text_.mnemonicOffset, text_.mnemonicLength), style, {});
        const int left = bounds.left + (bounds.Width() - full.size.width) / 2 + prefix.size.width;
        const int top = bounds.top + (bounds.Height() - full.size.height) / 2;
        const int underlineY = (std::min)(bounds.bottom - 2, top + full.baseline + 1);
        render::DuiPath underline;
        underline.MoveTo({left, underlineY});
        underline.LineTo({left + (std::max)(1, mnemonic.size.width), underlineY});
        canvas.StrokePath(underline, style.color, 1.0F);
    }

    ParsedButtonText text_;
    std::function<void()> clicked_;
    bool primary_{};
};
}

class DuiDialog::Impl {
public:
    enum class CaptionPart
    {
        None,
        Caption,
        Settings,
        Refresh,
        Help,
        Minimize,
        Maximize,
        Close,
    };

    class Surface final : public core::Control, public render::DuiRenderable
    {
    public:
        Surface(DuiDialog::Impl& owner, std::unique_ptr<core::Control> content)
            : owner_(owner), content_(content.get())
        {
            AddChild(std::move(content));
            for (std::size_t index = 0; index < owner_.buttons.size(); ++index)
            {
                auto button = std::make_unique<DialogButton>(owner_.buttons[index].text,
                    owner_.buttons[index].primary, [this, index] { owner_.ActivateButton(index); });
                buttons_.push_back(button.get());
                AddChild(std::move(button));
            }
            ApplyVisibility();
        }

        void Layout(core::Rect bounds)
        {
            SetBounds(bounds);
            if (!owner_.Decorated())
            {
                content_->SetBounds(bounds);
                return;
            }

            if (buttons_.empty())
            {
                panelBounds_ = {};
                const int pad = owner_.contentPadding >= 0 ? owner_.contentPadding : DialogPadding;
                const int topGap = owner_.contentPadding >= 0 ? owner_.contentPadding : 12;
                const int bottomGap = owner_.contentPadding >= 0 ? owner_.contentPadding
                                                                : DialogButtonBottom;
                content_->SetBounds({bounds.left + pad, bounds.top + TitleBarHeight + topGap,
                                     bounds.right - pad, bounds.bottom - bottomGap});
            }
            else
            {
                const int margin  = owner_.panelMargin;
                const int padding = owner_.panelPadding;
                const int buttonTop = bounds.bottom - DialogButtonBottom - DialogButtonHeight;
                panelBounds_ = {bounds.left + margin, bounds.top + TitleBarHeight + margin,
                                bounds.right - margin, buttonTop - DialogPanelGap};
                content_->SetBounds({panelBounds_.left + padding, panelBounds_.top + padding,
                                     panelBounds_.right - padding, panelBounds_.bottom - padding});
            }
            LayoutButtons(nullptr);
        }

        void ApplyVisibility()
        {
            content_->SetVisible(!owner_.collapsed);
            for (auto* button : buttons_)
                button->SetVisible(!owner_.collapsed);
        }

        core::Control* HitTest(core::Point point) override
        {
            if (!Visible() || !Bounds().Contains(point))
                return nullptr;
            const CaptionPart part = PartAt(point);
            hovered_ = IsAction(part) ? part : CaptionPart::None;
            // 标题栏命中归 Surface，避免落到内容区子控件导致假悬停。
            if (part != CaptionPart::None)
                return this;
            return core::Control::HitTest(point);
        }

        bool OnEvent(const core::Event& event) override
        {
            if (event.type == core::EventType::KeyDown)
            {
                if (event.key == core::key::Escape)
                {
                    owner_.Close(owner_.closeResult);
                    return true;
                }
                if (event.key == core::key::Enter)
                {
                    const auto primary = std::find_if(owner_.buttons.begin(), owner_.buttons.end(),
                        [](const DuiDialogButton& button) { return button.primary; });
                    if (primary != owner_.buttons.end())
                    {
                        owner_.ActivateButton(static_cast<std::size_t>(primary - owner_.buttons.begin()));
                        return true;
                    }
                }
            }

            // 中键按下：等同触发默认按钮（须先于标题栏/内容命中处理，否则被标题栏吞掉）
            if (event.type == core::EventType::PointerDown
                && event.button == core::PointerButton::Middle && owner_.middleClickAccepts)
            {
                owner_.ActivatePrimaryButton();
                return true;
            }

            if (!owner_.Decorated())
            {
                if (content_->OnEvent(event))
                    return true;
                // 无标题栏：空白区拖动窗口
                if (event.type == core::EventType::PointerDown
                    && event.button == core::PointerButton::Primary && owner_.popup != nullptr)
                {
                    owner_.popup->RequestMove();
                    return true;
                }
                return false;
            }

            const CaptionPart part = PartAt(event.position);
            if (event.type == core::EventType::PointerLeave)
            {
                hovered_ = CaptionPart::None;
                // 同步清内容区悬停，避免标题栏操作后旧按钮高亮复现。
                if (content_ != nullptr)
                    (void)content_->OnEvent(event);
                return true;
            }
            if (event.type == core::EventType::PointerMove)
            {
                hovered_ = part;
                if (part != CaptionPart::None && content_ != nullptr)
                {
                    // 默认构造再赋值：避免 -Wmissing-field-initializers（Event 含 text 等后置字段）。
                    core::Event leave{};
                    leave.type = core::EventType::PointerLeave;
                    leave.position = event.position;
                    (void)content_->OnEvent(leave);
                }
                return Captured() || part != CaptionPart::None;
            }
            if (event.type == core::EventType::PointerDown && part != CaptionPart::None)
            {
                // 标题栏任意按下：先清内容区悬停。
                if (content_ != nullptr)
                {
                    core::Event leave{};
                    leave.type = core::EventType::PointerLeave;
                    leave.position = event.position;
                    (void)content_->OnEvent(leave);
                }
                if (IsAction(part))
                {
                    pressed_ = part;
                    SetCaptured(true);
                    return true;
                }
                // 标题文字区：客户区发起拖动，保证 Move/Leave 仍走 HTCLIENT。
                if (part == CaptionPart::Caption && owner_.popup != nullptr)
                    owner_.popup->RequestMove();
                return true;
            }
            if (event.type == core::EventType::PointerCancel && Captured())
            {
                pressed_ = CaptionPart::None;
                hovered_ = CaptionPart::None;
                SetCaptured(false);
                return true;
            }
            if (event.type == core::EventType::PointerUp && Captured())
            {
                SetCaptured(false);
                const CaptionPart pressed = pressed_;
                pressed_ = CaptionPart::None;
                if (pressed == part)
                    Activate(pressed);
                return true;
            }
            if (event.type == core::EventType::PointerDoubleClick && part == CaptionPart::Caption)
            {
                hovered_ = CaptionPart::None;
                if (owner_.maximizable)
                {
                    owner_.ToggleMaximized();
                }
                else if (owner_.collapsible && !owner_.autoCollapse
                         && !(owner_.edgeAutoHide && owner_.EdgeDocked()))
                {
                    // 启用「自动折叠」时折叠态由自动行为驱动，双击标题栏不再干预；
                    // 「贴边隐藏」仅在实际贴边时才接管折叠（未贴边时仍允许双击折叠），
                    // 避免与自动行为互相打架
                    owner_.ToggleCollapsed();
                }
                return true;
            }
            return false;
        }

        void Paint(render::Canvas& canvas, core::Rect dirty) const override
        {
            if (!EffectivelyVisible())
                return;
            const core::Rect clipped = core::Rect::Intersect(Bounds(), dirty);
            if (clipped.Empty())
                return;
            const core::DuiTheme& theme = Theme();
            if (!owner_.Decorated())
            {
                canvas.FillRect(clipped, theme.Get(core::ThemeSlot::DialogBackground));
                render::PaintChildren(*this, canvas, dirty);
                return;
            }

            canvas.FillRect(clipped, theme.Get(core::ThemeSlot::DialogBackground));
            // 外框边框：与背景同色即等同不绘制（面板类窗口要求无可见边框时用）
            if (!owner_.collapsed && owner_.borderVisible)
                canvas.StrokeRoundedRect(Bounds(), 0, theme.Get(core::ThemeSlot::DialogBorder), 1.0F);
            const core::Rect titleBounds{Bounds().left, Bounds().top, Bounds().right,
                                         (std::min)(Bounds().bottom, Bounds().top + TitleBarHeight)};
            canvas.FillRect(titleBounds, theme.Get(core::ThemeSlot::DialogTitleBackground));
            PaintCaptionPart(canvas, SettingsRect(), CaptionPart::Settings);
            PaintCaptionPart(canvas, RefreshRect(), CaptionPart::Refresh);
            PaintCaptionPart(canvas, HelpRect(), CaptionPart::Help);
            PaintCaptionPart(canvas, MinimizeRect(), CaptionPart::Minimize);
            PaintCaptionPart(canvas, MaximizeRect(), CaptionPart::Maximize);
            PaintCaptionPart(canvas, CloseRect(), CaptionPart::Close);

            const int titleLeft = !owner_.maximizable ? Bounds().left + TitleBarHeight + 6 : Bounds().left + 10;
            const int trailingButtons = owner_.TrailingCaptionButtonCount();
            const int titleRight = Bounds().right - TitleBarHeight * trailingButtons - 4;
            const render::DuiTextStyle titleStyle{theme.Get(core::ThemeSlot::DialogTitleText),
                                                   "Segoe UI, Arial, SimSun", 9, false};
            canvas.DrawText(owner_.title,
                            {titleLeft, titleBounds.top, titleRight, titleBounds.bottom}, titleStyle,
                            render::DuiTextAlignment::Start, false);
            if (!owner_.collapsed && !panelBounds_.Empty())
            {
                if (owner_.panelBackground)
                    canvas.FillRect(panelBounds_, theme.Get(core::ThemeSlot::DialogPanelBackground));
                // 面板轮廓与窗口轮廓同受 borderVisible 控制（面板类窗口要求整体无线条）
                if (owner_.borderVisible)
                    canvas.StrokeRoundedRect(panelBounds_, 0,
                                             theme.Get(core::ThemeSlot::DialogPanelBorder), 1.0F);
            }
            LayoutButtons(&canvas);
            render::PaintChildren(*this, canvas, dirty);
        }

    private:
        [[nodiscard]] core::Rect SettingsRect() const
        {
            if (owner_.maximizable)
                return {};
            return {Bounds().left, Bounds().top, Bounds().left + TitleBarHeight,
                    Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] core::Rect RefreshRect() const
        {
            if (owner_.maximizable)
                return {};
            const int right = Bounds().right
                - (owner_.showHelpButton ? TitleBarHeight * 2 : TitleBarHeight);
            return {right - TitleBarHeight, Bounds().top, right, Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] core::Rect HelpRect() const
        {
            if (owner_.maximizable || !owner_.showHelpButton)
                return {};
            return {Bounds().right - TitleBarHeight * 2, Bounds().top,
                    Bounds().right - TitleBarHeight, Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] core::Rect CloseRect() const
        {
            return {Bounds().right - TitleBarHeight, Bounds().top, Bounds().right,
                    Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] core::Rect MaximizeRect() const
        {
            if (!owner_.maximizable)
                return {};
            return {Bounds().right - TitleBarHeight * 2, Bounds().top,
                    Bounds().right - TitleBarHeight, Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] core::Rect MinimizeRect() const
        {
            if (!owner_.maximizable)
                return {};
            return {Bounds().right - TitleBarHeight * 3, Bounds().top,
                    Bounds().right - TitleBarHeight * 2, Bounds().top + TitleBarHeight};
        }

        [[nodiscard]] CaptionPart PartAt(core::Point point) const
        {
            const core::Rect titleBounds{Bounds().left, Bounds().top, Bounds().right,
                                         Bounds().top + TitleBarHeight};
            if (!titleBounds.Contains(point))
                return CaptionPart::None;
            if (SettingsRect().Contains(point))
                return CaptionPart::Settings;
            if (RefreshRect().Contains(point))
                return CaptionPart::Refresh;
            if (HelpRect().Contains(point))
                return CaptionPart::Help;
            if (MinimizeRect().Contains(point))
                return CaptionPart::Minimize;
            if (CloseRect().Contains(point))
                return CaptionPart::Close;
            if (MaximizeRect().Contains(point))
                return CaptionPart::Maximize;
            return CaptionPart::Caption;
        }

        [[nodiscard]] static bool IsAction(CaptionPart part)
        {
            return part == CaptionPart::Settings || part == CaptionPart::Refresh
                || part == CaptionPart::Help || part == CaptionPart::Minimize
                || part == CaptionPart::Maximize || part == CaptionPart::Close;
        }

        struct CaptionIcons final
        {
            CaptionIconImages settings;
            CaptionIconImages refresh;
            CaptionIconImages help;
            CaptionIconImages minimize;
            CaptionIconImages maximize;
            CaptionIconImages restore;
            CaptionIconImages close;
        };

        void EnsureCaptionIcons() const
        {
            const core::DuiTheme& theme = Theme();
            if (iconTheme_ == &theme && iconThemeVersion_ == theme.Version())
                return;
            const core::Color normal = theme.Get(core::ThemeSlot::DialogTitleText);
            const core::Color hover = theme.Get(core::ThemeSlot::DialogCaptionHoverText);
            captionIcons_.settings = CreateCaptionIcons(detail::DialogSettingsSvg, normal, hover);
            captionIcons_.refresh = CreateCaptionIcons(detail::DialogRefreshSvg, normal, hover);
            captionIcons_.help = CreateCaptionIcons(detail::DialogHelpSvg, normal, hover);
            // 最小化/最大化/还原/关闭走共享图标（悬浮窗标题栏也用同一套）
            captionIcons_.minimize = {render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Minimize, normal),
                                      render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Minimize, hover)};
            captionIcons_.maximize = {render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Maximize, normal),
                                      render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Maximize, hover)};
            captionIcons_.restore = {render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Restore, normal),
                                     render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Restore, hover)};
            captionIcons_.close = {render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Close, normal),
                                   render::CreateCaptionGlyphImage(render::DuiCaptionGlyph::Close, hover)};
            iconTheme_ = &theme;
            iconThemeVersion_ = theme.Version();
        }

        void Activate(CaptionPart part)
        {
            if (part == CaptionPart::Settings && owner_.settings)
                owner_.settings();
            else if (part == CaptionPart::Refresh && owner_.refresh)
                owner_.refresh();
            else if (part == CaptionPart::Help && owner_.help)
                owner_.help();
            else if (part == CaptionPart::Minimize && owner_.popup)
                owner_.popup->RequestMinimize();
            else if (part == CaptionPart::Maximize)
                owner_.ToggleMaximized();
            else if (part == CaptionPart::Close)
                owner_.Close(owner_.closeResult);
        }

        void PaintCaptionPart(render::Canvas& canvas, core::Rect bounds, CaptionPart part) const
        {
            if (bounds.Empty())
                return;
            const core::DuiTheme& theme = Theme();
            if (pressed_ == part)
                canvas.FillRect(bounds, part == CaptionPart::Close
                    ? theme.Get(core::ThemeSlot::DialogClosePressed)
                    : theme.Get(core::ThemeSlot::DialogCaptionPressed));
            else if (hovered_ == part)
                canvas.FillRect(bounds, part == CaptionPart::Close
                    ? theme.Get(core::ThemeSlot::DialogCloseHover)
                    : theme.Get(core::ThemeSlot::DialogCaptionHover));
            EnsureCaptionIcons();
            const CaptionIconImages* icons{};
            if (part == CaptionPart::Settings) icons = &captionIcons_.settings;
            else if (part == CaptionPart::Refresh) icons = &captionIcons_.refresh;
            else if (part == CaptionPart::Help) icons = &captionIcons_.help;
            else if (part == CaptionPart::Minimize) icons = &captionIcons_.minimize;
            else if (part == CaptionPart::Maximize) icons = owner_.maximized
                ? &captionIcons_.restore : &captionIcons_.maximize;
            else if (part == CaptionPart::Close) icons = &captionIcons_.close;
            if (icons != nullptr)
            {
                const auto& icon = hovered_ == part && part != CaptionPart::Close
                    ? icons->hover : icons->normal;
                if (!icon)
                    return;
                const int left = bounds.left + (bounds.Width() - 16) / 2;
                const int top = bounds.top + (bounds.Height() - 16) / 2;
                canvas.DrawImage(*icon, {left, top, left + 16, top + 16});
            }
        }

        void LayoutButtons(render::DuiTextMeasurer* measurer) const
        {
            int right = Bounds().right - DialogPadding;
            for (auto iterator = buttons_.rbegin(); iterator != buttons_.rend(); ++iterator)
            {
                const int width = measurer == nullptr ? DialogButtonWidth : (*iterator)->DesiredWidth(*measurer);
                (*iterator)->SetBounds({right - width,
                                        Bounds().bottom - DialogButtonBottom - DialogButtonHeight,
                                        right, Bounds().bottom - DialogButtonBottom});
                right -= width + DialogButtonGap;
            }
        }

        DuiDialog::Impl& owner_;
        core::Control* content_{};
        std::vector<DialogButton*> buttons_;
        core::Rect panelBounds_;
        CaptionPart hovered_{CaptionPart::None};
        CaptionPart pressed_{CaptionPart::None};
        mutable CaptionIcons captionIcons_;
        mutable const core::DuiTheme* iconTheme_{};
        mutable unsigned iconThemeVersion_{};
    };

    [[nodiscard]] bool Decorated() const
    {
        if (!titleBarVisible)
            return false;
        return !title.empty() || !buttons.empty() || collapsible || maximizable
            || static_cast<bool>(settings) || static_cast<bool>(refresh) || static_cast<bool>(help);
    }

    [[nodiscard]] int TrailingCaptionButtonCount() const
    {
        if (maximizable)
            return 3;
        return showHelpButton ? 3 : 2;
    }

    [[nodiscard]] std::optional<ui::DuiPopupChrome> PopupChrome() const
    {
        if (Decorated())
        {
            return ui::DuiPopupChrome{TitleBarHeight, 8,
                                      maximizable ? 0 : TitleBarHeight,
                                      TitleBarHeight * TrailingCaptionButtonCount(), true};
        }
        // 无标题栏但可缩放：仍走自绘客户区（NCCALCSIZE=0 + 边缘命中），
        // 避免系统 WS_THICKFRAME 侵占客户区导致内容裁切、拖拽后布局错位。
        if (normalOptions.resizable)
            return ui::DuiPopupChrome{0, 8, 0, 0, true};
        return std::nullopt;
    }

    void Finish(int value)
    {
        if (!visible)
            return;
        visible = false;
        result = value;
        if (popup)
            popup->SetDismissedHandler({});
    }

    void Close(int value)
    {
        if (!visible)
            return;
        Finish(value);
        const auto handler = closed;
        if (popup)
            popup->RequestHide();
        if (handler)
            handler(value);
    }

    void ActivateButton(std::size_t index)
    {
        if (index < buttons.size() && buttons[index].result != IdNone)
            Close(buttons[index].result);
    }

    /** 触发默认（主）按钮；无默认按钮时不做任何事 */
    void ActivatePrimaryButton()
    {
        const auto primary = std::find_if(buttons.begin(), buttons.end(),
            [](const DuiDialogButton& button) { return button.primary; });
        if (primary != buttons.end())
            ActivateButton(static_cast<std::size_t>(primary - buttons.begin()));
    }

    void ApplyState()
    {
        if (surface)
            surface->ApplyVisibility();
        if (!popup || !visible)
            return;
        ui::DuiPopupOptions options = normalOptions;
        options.chrome = PopupChrome();
        if (collapsed)
        {
            options.size.height = TitleBarHeight;
            options.sizeMode = ui::DuiPopupSizeMode::Specified;
            options.resizable = false;
        }
        else if (maximized)
        {
            options.sizeMode = ui::DuiPopupSizeMode::WorkArea;
            options.resizable = true;
        }
        popup->SetOptions(options);
    }

    void ToggleCollapsed()
    {
        if (!collapsible || maximizable)
            return;
        collapsed = !collapsed;
        ApplyState();
    }

    void SetCollapsed(bool value)
    {
        if (!collapsible || maximizable)
            return;
        if (collapsed == value)
            return;
        collapsed = value;
        ApplyState();
    }

    /** 把自动折叠配置下发到宿主（宿主不可用时忽略） */
    void ApplyAutoCollapse()
    {
        if (!popup)
            return;
        const bool active = autoCollapse && collapsible && !maximizable;
        popup->SetAutoCollapse(active, autoCollapseExpandDelayMs, autoCollapseCollapseDelayMs,
                               collapsed,
                               [this](bool collapse) { SetCollapsed(collapse); });
    }

    /** 把贴边隐藏配置下发到宿主（宿主不可用时忽略） */
    void ApplyEdgeAutoHide()
    {
        if (!popup)
            return;
        ui::DuiEdgeAutoHideOptions options;
        options.enabled = edgeAutoHide && collapsible && !maximizable;
        options.referenceWindow = edgeReferenceWindow;
        options.slideOutMs = edgeSlideOutMs;
        options.hideMs = edgeHideMs;
        options.leaveDelayMs = edgeLeaveDelayMs;
        popup->SetEdgeAutoHide(options);

        // 收起为细边时转交上层（关闭已打开的菜单等依附界面）
        const auto hidden = edgeHidden;
        popup->SetEdgeHiddenHandler([hidden] {
            if (hidden)
                hidden();
        });
        // 折叠态即将贴边收起时先展开，使收起位置按展开后的窗口尺寸计算
        popup->SetEdgeHidePrepareHandler([this] {
            if (collapsed)
                SetCollapsed(false);
        });
    }

    /** @return 是否处于贴边状态（已收起为细边或已落位待收起） */
    [[nodiscard]] bool EdgeDocked() const { return popup != nullptr && popup->EdgeDocked(); }

    /** @return 弹出窗口原生句柄（无窗口时 0） */
    [[nodiscard]] std::uintptr_t NativeHandle() const
    {
        return popup != nullptr ? popup->NativeHandle() : 0;
    }

    void ToggleMaximized()
    {
        if (!maximizable)
            return;
        collapsed = false;
        maximized = !maximized;
        ApplyState();
    }

    std::unique_ptr<ui::IPopupHost> popup;
    Surface* surface{};
    std::string title;
    std::vector<DuiDialogButton> buttons;
    std::function<void(int)> closed;
    std::function<void()> settings;
    std::function<void()> refresh;
    std::function<void()> help;
    ui::DuiPopupOptions normalOptions;
    int result{IdNone};
    int closeResult{IdCancel};
    bool collapsible{};
    bool maximizable{};
    bool showHelpButton{true}; // 无最大化按钮时是否显示帮助按钮
    bool titleBarVisible{true}; // 是否显示标题栏
    bool followOwnerWindow{};
    bool collapsed{};
    bool maximized{};
    bool visible{};
    bool autoCollapse{};                 // 是否启用「光标移出自动折叠」
    int autoCollapseExpandDelayMs{200};  // 展开驻留延时（毫秒）
    int autoCollapseCollapseDelayMs{250};// 折叠驻留延时（毫秒）
    bool edgeAutoHide{};                 // 是否启用「贴边自动隐藏」（与 autoCollapse 互斥）
    std::uintptr_t edgeReferenceWindow{};// 贴边判定参考窗口；0=所在显示器工作区
    int edgeSlideOutMs{200};             // 滑出动画时长（毫秒）
    int edgeHideMs{250};                 // 收起动画时长（毫秒）
    int edgeLeaveDelayMs{250};           // 移出后收起延时（毫秒）
    std::function<void()> edgeHidden;    // 已贴边收起为细边的回调
    int contentPadding{-1}; // 无底栏时内容边距；<0 用默认 DialogPadding/12
    int panelMargin{DialogPanelMargin};   // 有底栏时面板相对窗口的内缩（左右 / 标题栏下）
    int panelPadding{DialogPanelPadding}; // 有底栏时内容相对面板的内边距
    bool panelBackground{true};           // 面板是否绘制自身底色（false 时与对话框背景同色，仅留边框）
    bool borderVisible{true};             // 是否绘制窗口最外圈边框
    bool middleClickAccepts{false};       // 鼠标中键按下是否等同触发默认按钮
};

DuiDialog::DuiDialog() : impl_(std::make_unique<Impl>()) {}
DuiDialog::~DuiDialog() { Close(impl_->closeResult); }
DuiDialog::DuiDialog(DuiDialog&&) noexcept = default;
DuiDialog& DuiDialog::operator=(DuiDialog&&) noexcept = default;

void DuiDialog::SetTitle(std::string title) { impl_->title = std::move(title); }
void DuiDialog::SetTitleBarVisible(bool visible)
{
    if (impl_->titleBarVisible == visible)
        return;
    impl_->titleBarVisible = visible;
    if (!visible && impl_->collapsed)
        impl_->collapsed = false;
    impl_->ApplyState();
}
bool DuiDialog::TitleBarVisible() const { return impl_->titleBarVisible; }

std::uintptr_t DuiDialog::NativeHandle() const
{
    return impl_->popup != nullptr ? impl_->popup->NativeHandle() : 0;
}

void DuiDialog::SetAnchor(core::Rect anchor)
{
    impl_->normalOptions.anchor = anchor;
    if (!impl_->popup || !impl_->visible)
        return;
    impl_->ApplyState();
}

void DuiDialog::SetOuterSize(core::Size size)
{
    if (size.width <= 0 || size.height <= 0)
        return;
    impl_->normalOptions.size = size;
    if (!impl_->popup || !impl_->visible)
        return;
    ui::DuiPopupOptions options = impl_->normalOptions;
    options.chrome = impl_->PopupChrome();
    if (impl_->collapsed)
    {
        options.size.height = TitleBarHeight;
        options.sizeMode = ui::DuiPopupSizeMode::Specified;
        options.resizable = false;
    }
    else if (impl_->maximized)
    {
        options.sizeMode = ui::DuiPopupSizeMode::WorkArea;
        options.resizable = true;
    }
    impl_->popup->SetOptions(options);
}

void DuiDialog::SetButtons(std::vector<DuiDialogButton> buttons) { impl_->buttons = std::move(buttons); }void DuiDialog::SetContentPadding(int padding) { impl_->contentPadding = padding; }
void DuiDialog::SetPanelInsets(int margin, int padding)
{
    impl_->panelMargin  = (std::max)(0, margin);
    impl_->panelPadding = (std::max)(0, padding);
    if (impl_->surface)
        impl_->surface->Layout(impl_->surface->Bounds());
}
void DuiDialog::SetPanelBackgroundVisible(bool visible) { impl_->panelBackground = visible; }
void DuiDialog::SetBorderVisible(bool visible) { impl_->borderVisible = visible; }
void DuiDialog::SetMiddleClickAccepts(bool accepts) { impl_->middleClickAccepts = accepts; }
void DuiDialog::SetCollapsible(bool collapsible)
{
    impl_->collapsible = collapsible;
    if (!collapsible && impl_->collapsed)
    {
        impl_->collapsed = false;
        impl_->ApplyState();
    }
}
void DuiDialog::SetMaximizable(bool maximizable)
{
    impl_->maximizable = maximizable;
    if (!maximizable && impl_->maximized)
    {
        impl_->maximized = false;
        impl_->ApplyState();
    }
    if (maximizable && impl_->collapsed)
    {
        impl_->collapsed = false;
        impl_->ApplyState();
    }
}
void DuiDialog::SetFollowOwnerWindow(bool follow)
{
    if (impl_->followOwnerWindow == follow)
        return;
    impl_->followOwnerWindow = follow;
    impl_->normalOptions.followOwner = follow;
    impl_->ApplyState();
}
bool DuiDialog::FollowOwnerWindow() const { return impl_->followOwnerWindow; }
void DuiDialog::SetSettingsHandler(std::function<void()> handler) { impl_->settings = std::move(handler); }
void DuiDialog::SetRefreshHandler(std::function<void()> handler) { impl_->refresh = std::move(handler); }
void DuiDialog::SetHelpHandler(std::function<void()> handler) { impl_->help = std::move(handler); }
void DuiDialog::SetShowHelpButton(bool show)
{
    if (impl_->showHelpButton == show)
        return;
    impl_->showHelpButton = show;
    impl_->ApplyState();
}

ui::HostRef DuiDialog::HostReference() const
{
    if (!impl_->popup)
        return {};
    return impl_->popup->Reference();
}

core::Rect DuiDialog::SettingsButtonRect() const
{
    // 与 Surface::SettingsRect 对齐：标题栏左侧正方形设置按钮。
    constexpr int kTitleBarHeight = 24;
    if (!Visible() || impl_->maximizable)
        return {};
    return {0, 0, kTitleBarHeight, kTitleBarHeight};
}

void DuiDialog::RequestMove()
{
    if (impl_->popup != nullptr && Visible())
        impl_->popup->RequestMove();
}

bool DuiDialog::Show(ui::IUiHostFactory& factory, ui::HostRef owner,
                     ui::DuiPopupOptions options, std::unique_ptr<core::Control> content)
{
    if (!content || options.size.width <= 0 || options.size.height <= 0)
        return false;
    Close(impl_->closeResult);
    impl_->popup = factory.CreatePopupHost(owner);
    if (!impl_->popup)
        return false;

    impl_->collapsed = false;
    impl_->maximized = false;
    options.followOwner = impl_->followOwnerWindow;
    options.chrome = impl_->PopupChrome();
    impl_->normalOptions = options;
    auto surface = std::make_unique<Impl::Surface>(*impl_, std::move(content));
    Impl::Surface* rawSurface = surface.get();
    impl_->surface = rawSurface;
    impl_->result = IdNone;
    impl_->popup->SetDismissedHandler([state = impl_.get()] {
        const int result = state->closeResult;
        state->Finish(result);
        if (state->closed)
            state->closed(result);
    });
    if (!impl_->popup->Show(options, std::move(surface),
                            [rawSurface](core::Rect bounds) { rawSurface->Layout(bounds); },
                            [rawSurface](render::Canvas& canvas, core::Rect dirty) {
                                rawSurface->Paint(canvas, dirty);
                            })) {
        impl_->surface = nullptr;
        impl_->popup.reset();
        return false;
    }
    impl_->visible = true;
    // 宿主就绪后下发自动折叠与贴边隐藏配置
    impl_->ApplyAutoCollapse();
    impl_->ApplyEdgeAutoHide();
    return true;
}

void DuiDialog::Close(int result)
{
    if (impl_)
        impl_->Close(result);
}

void DuiDialog::SetCloseResult(int result) { impl_->closeResult = result; }
int DuiDialog::CloseResult() const { return impl_->closeResult; }
int DuiDialog::Result() const { return impl_->result; }
bool DuiDialog::Visible() const { return impl_->visible; }
bool DuiDialog::Collapsed() const { return impl_->collapsed; }
bool DuiDialog::Maximized() const { return impl_->maximized; }
void DuiDialog::ToggleCollapsed() { impl_->ToggleCollapsed(); }
void DuiDialog::SetCollapsed(bool collapsed) { impl_->SetCollapsed(collapsed); }
void DuiDialog::SetAutoCollapse(bool enabled, int expandDelayMs, int collapseDelayMs)
{
    impl_->autoCollapse = enabled;
    impl_->autoCollapseExpandDelayMs = expandDelayMs;
    impl_->autoCollapseCollapseDelayMs = collapseDelayMs;
    // 与贴边隐藏互斥：同一时刻只保留一种自动行为
    if (enabled)
    {
        impl_->edgeAutoHide = false;
        impl_->ApplyEdgeAutoHide();
    }
    impl_->ApplyAutoCollapse();
}
bool DuiDialog::AutoCollapse() const { return impl_->autoCollapse; }

void DuiDialog::SetEdgeAutoHide(bool enabled, std::uintptr_t referenceWindow, int slideOutMs,
                                int hideMs, int leaveDelayMs)
{
    impl_->edgeAutoHide = enabled;
    impl_->edgeReferenceWindow = referenceWindow;
    impl_->edgeSlideOutMs = slideOutMs;
    impl_->edgeHideMs = hideMs;
    impl_->edgeLeaveDelayMs = leaveDelayMs;
    // 与自动折叠互斥：同一时刻只保留一种自动行为
    if (enabled)
    {
        impl_->autoCollapse = false;
        impl_->ApplyAutoCollapse();
    }
    impl_->ApplyEdgeAutoHide();
}
bool DuiDialog::EdgeAutoHide() const { return impl_->edgeAutoHide; }

bool DuiDialog::EdgeDocked() const { return impl_->EdgeDocked(); }

void DuiDialog::SetEdgeHiddenHandler(std::function<void()> handler)
{
    impl_->edgeHidden = std::move(handler);
}
void DuiDialog::TryDockEdgeNow()
{
    if (impl_->popup)
        impl_->popup->TryDockEdgeNow();
}
void DuiDialog::ToggleMaximized() { impl_->ToggleMaximized(); }
void DuiDialog::SetClosedHandler(std::function<void(int)> handler) { impl_->closed = std::move(handler); }

} // namespace ysDui::controls::window
