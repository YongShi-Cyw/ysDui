#include "ysDui/core/DuiTheme.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace ysDui::core {
namespace {
constexpr std::size_t slotCount = static_cast<std::size_t>(ThemeSlot::ImageViewerBackdrop) + 1;

std::size_t indexOf(ThemeSlot slot)
{
    return static_cast<std::size_t>(slot);
}

Color rgb(unsigned char red, unsigned char green, unsigned char blue)
{
    return {red, green, blue, 255};
}

/** 构造带透明度的颜色（rgb 的 alpha 版）。 */
Color rgba(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
    return {red, green, blue, alpha};
}

template <std::size_t Size>
void setColor(std::array<Color, Size>& colors, ThemeSlot slot, Color color)
{
    colors[indexOf(slot)] = color;
}

template <std::size_t Size>
void copyColor(std::array<Color, Size>& colors, ThemeSlot target, ThemeSlot source)
{
    setColor(colors, target, colors[indexOf(source)]);
}

template <std::size_t Size>
void applyDerivedColors(std::array<Color, Size>& colors, ThemeSlot visitedLinkSource,
                        ThemeSlot toastBackgroundSource)
{
    constexpr std::pair<ThemeSlot, ThemeSlot> mappings[]{
        {ThemeSlot::SwitchKnobBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::BadgeBackground, ThemeSlot::Danger},
        {ThemeSlot::ButtonGhostHover, ThemeSlot::RowHover},
        {ThemeSlot::ButtonGhostPressed, ThemeSlot::ControlPressed},
        {ThemeSlot::ButtonOutlinedHover, ThemeSlot::RowHover},
        {ThemeSlot::ButtonOutlinedPressed, ThemeSlot::ControlPressed},
        {ThemeSlot::ButtonPrimaryHover, ThemeSlot::BrandHover},
        {ThemeSlot::ButtonPrimaryPressed, ThemeSlot::BrandPressed},
        {ThemeSlot::ButtonText, ThemeSlot::TextDefault},
        {ThemeSlot::ButtonDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::ButtonChoiceDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::ButtonChoiceGhostText, ThemeSlot::TextSubtle},
        {ThemeSlot::ButtonChoiceBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::BreadcrumbText, ThemeSlot::TextDefault},
        {ThemeSlot::BreadcrumbSeparator, ThemeSlot::TextSubtle},
        {ThemeSlot::PanelHeaderBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::PanelBorder, ThemeSlot::BorderLight},
        {ThemeSlot::PanelText, ThemeSlot::TextDefault},
        {ThemeSlot::PanelGlyph, ThemeSlot::TextSubtle},
        {ThemeSlot::GroupBorder, ThemeSlot::BorderLight},
        {ThemeSlot::GroupText, ThemeSlot::TextDefault},
        {ThemeSlot::SegmentedBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::SegmentedText, ThemeSlot::TextDefault},
        {ThemeSlot::StatusBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::ToolbarBackground, ThemeSlot::ControlBackground},
        {ThemeSlot::ProgressTrack, ThemeSlot::BorderLight},
        {ThemeSlot::ProgressText, ThemeSlot::TextDefault},
        {ThemeSlot::StatusText, ThemeSlot::TextDefault},
        {ThemeSlot::AvatarStatusOnline, ThemeSlot::StatusOnline},
        {ThemeSlot::AvatarStatusAway, ThemeSlot::StatusAway},
        {ThemeSlot::AvatarStatusBusy, ThemeSlot::StatusBusy},
        {ThemeSlot::AvatarStatusOffline, ThemeSlot::StatusOffline},
        {ThemeSlot::FieldDisabledBackground, ThemeSlot::ControlDisabled},
        {ThemeSlot::FieldPlainDisabledBackground, ThemeSlot::ControlDisabled},
        {ThemeSlot::FieldBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::FieldDisabledBorder, ThemeSlot::ControlBorder},
        {ThemeSlot::FieldFocusBorder, ThemeSlot::BrandBorder},
        {ThemeSlot::FieldHoverBorder, ThemeSlot::BrandHover},
        {ThemeSlot::FieldText, ThemeSlot::TextDefault},
        {ThemeSlot::FieldDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::FieldPlaceholder, ThemeSlot::TextSubtle},
        {ThemeSlot::SpinText, ThemeSlot::TextDefault},
        {ThemeSlot::SpinDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::SpinButtonHover, ThemeSlot::RowHover},
        {ThemeSlot::SpinButtonPressed, ThemeSlot::ControlPressed},
        {ThemeSlot::InputArrow, ThemeSlot::TextSubtle},
        {ThemeSlot::ComboArrow, ThemeSlot::TextSubtle},
        {ThemeSlot::ControlText, ThemeSlot::TextDefault},
        {ThemeSlot::SearchPlaceholder, ThemeSlot::TextSubtle},
        {ThemeSlot::SearchGlyph, ThemeSlot::TextSubtle},
        {ThemeSlot::SearchClear, ThemeSlot::TextDisabled},
        {ThemeSlot::InputButtonBackground, ThemeSlot::ControlBackground},
        {ThemeSlot::InputButtonDisabledBackground, ThemeSlot::ControlDisabled},
        {ThemeSlot::PopupBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::ColorGridBorder, ThemeSlot::BorderLight},
        {ThemeSlot::ColorPickerArrow, ThemeSlot::TextSubtle},
        {ThemeSlot::DateTimeArrow, ThemeSlot::TextSubtle},
        {ThemeSlot::SliderTrack, ThemeSlot::BorderLight},
        {ThemeSlot::SliderDisabledThumb, ThemeSlot::ControlDisabled},
        {ThemeSlot::SliderBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::SliderDisabledBorder, ThemeSlot::ControlBorder},
        {ThemeSlot::ScrollTrack, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::ScrollThumb, ThemeSlot::ControlBorder},
        {ThemeSlot::ListBackground, ThemeSlot::SurfaceBackground},
        {ThemeSlot::ListSelection, ThemeSlot::SelectionBackground},
        {ThemeSlot::ListSelectionText, ThemeSlot::TextOnSelectedRow},
        {ThemeSlot::ListHover, ThemeSlot::RowHover},
        {ThemeSlot::ListText, ThemeSlot::TextDefault},
        {ThemeSlot::ListCheckboxBorder, ThemeSlot::ControlBorder},
        {ThemeSlot::ListCheckboxFill, ThemeSlot::BrandPrimary},
        {ThemeSlot::ListInsertion, ThemeSlot::BrandPrimary},
        {ThemeSlot::GridBackground, ThemeSlot::SurfaceBackground},
        {ThemeSlot::GridBorder, ThemeSlot::BorderLight},
        {ThemeSlot::GridHeaderBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::GridLine, ThemeSlot::BorderLight},
        {ThemeSlot::GridSelection, ThemeSlot::SelectionBackground},
        {ThemeSlot::SpreadsheetBackground, ThemeSlot::GridBackground},
        {ThemeSlot::SpreadsheetHeaderBackground, ThemeSlot::GridHeaderBackground},
        {ThemeSlot::SpreadsheetGridLine, ThemeSlot::GridLine},
        {ThemeSlot::SpreadsheetSelection, ThemeSlot::GridSelection},
        {ThemeSlot::SpreadsheetActiveCell, ThemeSlot::BrandPrimary},
        {ThemeSlot::TreeText, ThemeSlot::TextDefault},
        {ThemeSlot::TreeSubtitle, ThemeSlot::TextSubtle},
        {ThemeSlot::TreeSelection, ThemeSlot::SelectionBackground},
        {ThemeSlot::TreeZebra, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::TreeHover, ThemeSlot::RowHover},
        {ThemeSlot::TreeProgressTrack, ThemeSlot::BorderLight},
        {ThemeSlot::PropertyGroupBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::TabBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::TabIdle, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::TabHover, ThemeSlot::ControlHover},
        {ThemeSlot::TabSelected, ThemeSlot::SurfaceBackground},
        {ThemeSlot::TabText, ThemeSlot::TextDefault},
        {ThemeSlot::TabSelectedText, ThemeSlot::TextLink},
        {ThemeSlot::TabBorder, ThemeSlot::BorderLight},
        {ThemeSlot::TabCloseHoverBackground, ThemeSlot::Danger},
        {ThemeSlot::TabCloseHoverStroke, ThemeSlot::TextOnPrimary},
        {ThemeSlot::TabGlyph, ThemeSlot::TextSubtle},
        {ThemeSlot::TabDropdownHover, ThemeSlot::BrandHover},
        {ThemeSlot::TabScrollArrow, ThemeSlot::TextDefault},
        {ThemeSlot::TabScrollArrowDisabled, ThemeSlot::TextDisabled},
        {ThemeSlot::MenuBackground, ThemeSlot::SurfaceBackground},
        {ThemeSlot::MenuBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::MenuSeparator, ThemeSlot::BorderLight},
        {ThemeSlot::MenuHighlight, ThemeSlot::RowHover},
        {ThemeSlot::MenuText, ThemeSlot::TextDefault},
        {ThemeSlot::MenuDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::MenuBarActive, ThemeSlot::SelectionBackground},
        {ThemeSlot::MenuBarHover, ThemeSlot::ControlHover},
        {ThemeSlot::MenuBarText, ThemeSlot::TextDefault},
        {ThemeSlot::MenuBarDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::BusyTrack, ThemeSlot::BorderLight},
        {ThemeSlot::BusyIndicator, ThemeSlot::BrandPrimary},
        {ThemeSlot::EmojiText, ThemeSlot::TextDefault},
        {ThemeSlot::EmojiHover, ThemeSlot::RowHover},
        {ThemeSlot::EmojiPressed, ThemeSlot::SelectionBackground},
        {ThemeSlot::ToolTipBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::ToolTipBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::ToolTipText, ThemeSlot::TextDefault},
        {ThemeSlot::ScrollViewBackground, ThemeSlot::SurfaceBackground},
        {ThemeSlot::SplitterBar, ThemeSlot::BorderLight},
        {ThemeSlot::SplitterHover, ThemeSlot::BrandHover},
        {ThemeSlot::NativeHostBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::NativeHostBorder, ThemeSlot::BorderHeavy},
        {ThemeSlot::NativeHostText, ThemeSlot::TextDefault},
        {ThemeSlot::DialogBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::DialogTitleBackground, ThemeSlot::BrandPrimary},
        {ThemeSlot::DialogCaptionHover, ThemeSlot::ControlHover},
        {ThemeSlot::DialogCaptionPressed, ThemeSlot::ControlPressed},
        {ThemeSlot::DialogCloseHover, ThemeSlot::DangerHover},
        {ThemeSlot::DialogClosePressed, ThemeSlot::DangerPressed},
        {ThemeSlot::DialogTitleText, ThemeSlot::TextOnPrimary},
        {ThemeSlot::DialogCaptionHoverText, ThemeSlot::TextDefault},
        {ThemeSlot::DialogBorder, ThemeSlot::BorderLight},
        {ThemeSlot::DialogPanelBackground, ThemeSlot::SurfaceBackground},
        {ThemeSlot::DialogPanelBorder, ThemeSlot::BorderLight},
        {ThemeSlot::DialogAccent, ThemeSlot::BrandPrimary},
        {ThemeSlot::DialogButtonHover, ThemeSlot::RowHover},
        {ThemeSlot::DialogPrimaryHover, ThemeSlot::BrandHover},
        {ThemeSlot::DialogPrimaryPressed, ThemeSlot::BrandPressed},
        {ThemeSlot::DialogDisabledBackground, ThemeSlot::ControlDisabled},
        {ThemeSlot::DialogDisabledBorder, ThemeSlot::ControlBorder},
        {ThemeSlot::DialogDisabledText, ThemeSlot::TextDisabled},
        {ThemeSlot::DialogText, ThemeSlot::TextDefault},
        {ThemeSlot::MessageBackground, ThemeSlot::SurfaceAlternateBackground},
        {ThemeSlot::MessageWarningFill, ThemeSlot::StatusAway},
        {ThemeSlot::MessageWarningText, ThemeSlot::TextDefault},
        {ThemeSlot::MessageErrorFill, ThemeSlot::Danger},
        {ThemeSlot::MessageInformationFill, ThemeSlot::BrandPrimary},
        {ThemeSlot::MessageSuccessFill, ThemeSlot::StatusOnline},
    };
    for (const auto& [target, source] : mappings)
        copyColor(colors, target, source);
    copyColor(colors, ThemeSlot::LinkVisited, visitedLinkSource);
    copyColor(colors, ThemeSlot::ToastBackground, toastBackgroundSource);
}
} // namespace

class DuiTheme::Impl {
public:
    struct SubscriptionStore final {
        struct Subscription final {
            std::size_t token{};
            std::function<void()> callback;
        };

        std::size_t nextToken{1};
        std::vector<Subscription> subscriptions;
    };

    std::array<Color, slotCount> colors{};
    unsigned version{1};
    std::shared_ptr<SubscriptionStore> subscriptionStore{std::make_shared<SubscriptionStore>()};
    std::string fontFamily{"Microsoft YaHei"};
    int fontPointSize{9};
    bool focusRingVisible{}; // 焦点线框默认关闭
};

void DuiTheme::FillLight(Impl& theme)
{
    theme.colors = {rgb(45, 108, 223), rgb(37, 89, 184), rgb(30, 74, 153), rgb(30, 74, 153),
                    rgb(255, 255, 255), rgb(30, 30, 30), rgb(110, 110, 110), rgb(170, 170, 170),
                    rgb(45, 108, 223), rgb(255, 255, 255), rgb(245, 245, 248), rgb(220, 220, 224),
                    rgb(180, 180, 188), rgb(232, 240, 252), rgb(45, 108, 223), rgb(255, 255, 255),
                    rgb(60, 175, 80), rgb(240, 175, 40), rgb(220, 60, 60), rgb(150, 150, 150)};
    setColor(theme.colors, ThemeSlot::ControlBackground, rgb(248, 249, 251));
    setColor(theme.colors, ThemeSlot::ControlHover, rgb(235, 238, 244));
    setColor(theme.colors, ThemeSlot::ControlPressed, rgb(220, 224, 232));
    setColor(theme.colors, ThemeSlot::ControlDisabled, rgb(205, 207, 212));
    setColor(theme.colors, ThemeSlot::ControlBorder, rgb(190, 194, 202));
    setColor(theme.colors, ThemeSlot::SelectionBackground, rgb(217, 232, 252));
    // 文本输入框选中：采用 Windows 标准高亮蓝，白字保证对比度
    setColor(theme.colors, ThemeSlot::TextSelectionBackground, rgb(0, 120, 215));
    setColor(theme.colors, ThemeSlot::TextSelectionText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ToggleOn, rgb(7, 193, 96));
    setColor(theme.colors, ThemeSlot::ToggleOff, rgb(229, 229, 229));
    setColor(theme.colors, ThemeSlot::SwitchKnobBorder, rgb(180, 180, 180));
    setColor(theme.colors, ThemeSlot::Danger, rgb(214, 66, 72));
    setColor(theme.colors, ThemeSlot::DangerHover, rgb(185, 50, 55));
    setColor(theme.colors, ThemeSlot::DangerPressed, rgb(150, 35, 40));
    setColor(theme.colors, ThemeSlot::BadgeBackground, rgb(220, 60, 60));
    setColor(theme.colors, ThemeSlot::ButtonGhostHover, rgb(226, 236, 253));
    setColor(theme.colors, ThemeSlot::ButtonGhostPressed, rgb(204, 220, 248));
    setColor(theme.colors, ThemeSlot::ButtonOutlinedHover, rgb(235, 242, 255));
    setColor(theme.colors, ThemeSlot::ButtonOutlinedPressed, rgb(221, 233, 255));
    setColor(theme.colors, ThemeSlot::ButtonPrimaryHover, rgb(34, 88, 194));
    setColor(theme.colors, ThemeSlot::ButtonPrimaryPressed, rgb(29, 70, 157));
    setColor(theme.colors, ThemeSlot::ButtonText, rgb(35, 38, 44));
    setColor(theme.colors, ThemeSlot::ButtonDisabledText, rgb(245, 245, 247));
    setColor(theme.colors, ThemeSlot::ButtonChoiceDisabledText, rgb(145, 148, 154));
    setColor(theme.colors, ThemeSlot::ButtonChoiceGhostText, rgb(100, 108, 120));
    setColor(theme.colors, ThemeSlot::ButtonChoiceBorder, rgb(175, 180, 190));
    setColor(theme.colors, ThemeSlot::LinkVisited, rgb(112, 72, 168));
    setColor(theme.colors, ThemeSlot::BreadcrumbText, rgb(80, 80, 90));
    setColor(theme.colors, ThemeSlot::BreadcrumbSeparator, rgb(150, 150, 160));
    setColor(theme.colors, ThemeSlot::PanelHeaderBackground, rgb(240, 240, 244));
    setColor(theme.colors, ThemeSlot::PanelBorder, rgb(210, 210, 216));
    setColor(theme.colors, ThemeSlot::PanelText, rgb(40, 40, 50));
    setColor(theme.colors, ThemeSlot::PanelGlyph, rgb(75, 85, 99));
    setColor(theme.colors, ThemeSlot::GroupBorder, rgb(200, 200, 208));
    setColor(theme.colors, ThemeSlot::GroupText, rgb(60, 60, 80));
    setColor(theme.colors, ThemeSlot::SegmentedBackground, rgb(240, 242, 246));
    setColor(theme.colors, ThemeSlot::SegmentedText, rgb(60, 60, 70));
    setColor(theme.colors, ThemeSlot::StatusBackground, rgb(245, 245, 247));
    setColor(theme.colors, ThemeSlot::ToastBackground, rgb(50, 50, 50));
    setColor(theme.colors, ThemeSlot::ToolbarBackground, rgb(248, 248, 250));
    setColor(theme.colors, ThemeSlot::ProgressTrack, rgb(232, 232, 236));
    setColor(theme.colors, ThemeSlot::ProgressText, rgb(68, 72, 82));
    setColor(theme.colors, ThemeSlot::StatusText, rgb(60, 60, 70));
    setColor(theme.colors, ThemeSlot::AvatarStatusOnline, rgb(60, 200, 120));
    setColor(theme.colors, ThemeSlot::AvatarStatusAway, rgb(220, 170, 45));
    setColor(theme.colors, ThemeSlot::AvatarStatusBusy, rgb(220, 60, 60));
    setColor(theme.colors, ThemeSlot::AvatarStatusOffline, rgb(140, 145, 155));
    setColor(theme.colors, ThemeSlot::FieldDisabledBackground, rgb(240, 240, 242));
    setColor(theme.colors, ThemeSlot::FieldPlainDisabledBackground, rgb(240, 240, 240));
    setColor(theme.colors, ThemeSlot::FieldBorder, rgb(150, 150, 150));
    setColor(theme.colors, ThemeSlot::FieldDisabledBorder, rgb(190, 190, 190));
    setColor(theme.colors, ThemeSlot::FieldFocusBorder, rgb(80, 130, 200));
    setColor(theme.colors, ThemeSlot::FieldHoverBorder, rgb(100, 140, 200));
    setColor(theme.colors, ThemeSlot::FieldText, rgb(30, 30, 40));
    setColor(theme.colors, ThemeSlot::FieldDisabledText, rgb(140, 140, 150));
    setColor(theme.colors, ThemeSlot::FieldPlaceholder, rgb(150, 150, 160));
    setColor(theme.colors, ThemeSlot::SpinText, rgb(40, 40, 40));
    setColor(theme.colors, ThemeSlot::SpinDisabledText, rgb(140, 140, 140));
    setColor(theme.colors, ThemeSlot::SpinButtonHover, rgb(175, 215, 225));
    setColor(theme.colors, ThemeSlot::SpinButtonPressed, rgb(150, 200, 215));
    setColor(theme.colors, ThemeSlot::InputArrow, rgb(80, 80, 100));
    setColor(theme.colors, ThemeSlot::ComboArrow, rgb(80, 100, 140));
    setColor(theme.colors, ThemeSlot::ControlText, rgb(20, 20, 20));
    setColor(theme.colors, ThemeSlot::SearchPlaceholder, rgb(145, 145, 155));
    setColor(theme.colors, ThemeSlot::SearchGlyph, rgb(110, 110, 120));
    setColor(theme.colors, ThemeSlot::SearchClear, rgb(150, 150, 150));
    setColor(theme.colors, ThemeSlot::InputButtonBackground, rgb(240, 242, 246));
    setColor(theme.colors, ThemeSlot::InputButtonDisabledBackground, rgb(232, 232, 235));
    setColor(theme.colors, ThemeSlot::PopupBorder, rgb(170, 170, 170));
    setColor(theme.colors, ThemeSlot::ColorGridBorder, rgb(200, 200, 200));
    setColor(theme.colors, ThemeSlot::ColorPickerArrow, rgb(100, 100, 110));
    setColor(theme.colors, ThemeSlot::DateTimeArrow, rgb(80, 80, 90));
    setColor(theme.colors, ThemeSlot::SliderTrack, rgb(220, 220, 226));
    setColor(theme.colors, ThemeSlot::SliderDisabledThumb, rgb(242, 242, 245));
    setColor(theme.colors, ThemeSlot::SliderBorder, rgb(160, 165, 175));
    setColor(theme.colors, ThemeSlot::SliderDisabledBorder, rgb(210, 212, 218));
    setColor(theme.colors, ThemeSlot::ScrollTrack, rgb(240, 240, 240));
    setColor(theme.colors, ThemeSlot::ScrollThumb, rgb(205, 205, 205));
    setColor(theme.colors, ThemeSlot::ListBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ListSelection, rgb(180, 210, 245));
    setColor(theme.colors, ThemeSlot::ListSelectionText, rgb(10, 30, 90));
    setColor(theme.colors, ThemeSlot::ListHover, rgb(232, 240, 252));
    setColor(theme.colors, ThemeSlot::ListText, rgb(20, 20, 20));
    setColor(theme.colors, ThemeSlot::ListCheckboxBorder, rgb(150, 150, 160));
    setColor(theme.colors, ThemeSlot::ListCheckboxFill, rgb(60, 120, 220));
    setColor(theme.colors, ThemeSlot::ListInsertion, rgb(60, 120, 220));
    setColor(theme.colors, ThemeSlot::GridBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::GridBorder, rgb(200, 200, 208));
    setColor(theme.colors, ThemeSlot::GridHeaderBackground, rgb(245, 245, 248));
    setColor(theme.colors, ThemeSlot::GridLine, rgb(230, 230, 235));
    setColor(theme.colors, ThemeSlot::GridSelection, rgb(210, 230, 250));
    setColor(theme.colors, ThemeSlot::SpreadsheetBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::SpreadsheetHeaderBackground, rgb(245, 245, 248));
    setColor(theme.colors, ThemeSlot::SpreadsheetGridLine, rgb(230, 230, 230));
    setColor(theme.colors, ThemeSlot::SpreadsheetSelection, rgb(230, 230, 230));
    setColor(theme.colors, ThemeSlot::SpreadsheetActiveCell, rgb(45, 108, 223));
    setColor(theme.colors, ThemeSlot::TreeText, rgb(34, 34, 40));
    setColor(theme.colors, ThemeSlot::TreeSubtitle, rgb(110, 110, 120));
    setColor(theme.colors, ThemeSlot::TreeSelection, rgb(175, 215, 225)); // 与菜单悬停/高亮一致
    setColor(theme.colors, ThemeSlot::TreeZebra, rgb(250, 250, 252));
    setColor(theme.colors, ThemeSlot::TreeHover, rgb(175, 215, 225)); // 保留槽位；树形指令面板已关闭悬停绘制
    setColor(theme.colors, ThemeSlot::TreeProgressTrack, rgb(224, 229, 237));
    setColor(theme.colors, ThemeSlot::PropertyGroupBackground, rgb(240, 240, 244));
    setColor(theme.colors, ThemeSlot::TabBackground, rgb(238, 240, 244));
    setColor(theme.colors, ThemeSlot::TabIdle, rgb(238, 240, 244));
    setColor(theme.colors, ThemeSlot::TabHover, rgb(228, 232, 240));
    setColor(theme.colors, ThemeSlot::TabSelected, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::TabText, rgb(50, 50, 60));
    setColor(theme.colors, ThemeSlot::TabSelectedText, rgb(22, 93, 255));
    setColor(theme.colors, ThemeSlot::TabBorder, rgb(212, 215, 222));
    setColor(theme.colors, ThemeSlot::TabCloseHoverBackground, rgb(220, 100, 100));
    setColor(theme.colors, ThemeSlot::TabCloseHoverStroke, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::TabGlyph, rgb(110, 110, 110));
    setColor(theme.colors, ThemeSlot::TabDropdownHover, rgb(80, 130, 200));
    setColor(theme.colors, ThemeSlot::TabScrollArrow, rgb(80, 80, 100));
    setColor(theme.colors, ThemeSlot::TabScrollArrowDisabled, rgb(190, 190, 200));
    setColor(theme.colors, ThemeSlot::MenuBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::MenuBorder, rgb(170, 170, 170));
    setColor(theme.colors, ThemeSlot::MenuSeparator, rgb(224, 224, 228));
    setColor(theme.colors, ThemeSlot::MenuHighlight, rgb(175, 215, 225));
    setColor(theme.colors, ThemeSlot::MenuText, rgb(32, 32, 36));
    setColor(theme.colors, ThemeSlot::MenuDisabledText, rgb(150, 150, 154));
    setColor(theme.colors, ThemeSlot::MenuBarActive, rgb(215, 227, 244));
    setColor(theme.colors, ThemeSlot::MenuBarHover, rgb(229, 229, 229));
    setColor(theme.colors, ThemeSlot::MenuBarText, rgb(0, 0, 0));
    setColor(theme.colors, ThemeSlot::MenuBarDisabledText, rgb(160, 160, 160));
    setColor(theme.colors, ThemeSlot::BusyTrack, rgb(220, 224, 230));
    setColor(theme.colors, ThemeSlot::BusyIndicator, rgb(45, 108, 223));
    setColor(theme.colors, ThemeSlot::EmojiText, rgb(20, 20, 20));
    setColor(theme.colors, ThemeSlot::EmojiHover, rgb(232, 240, 252));
    setColor(theme.colors, ThemeSlot::EmojiPressed, rgb(200, 220, 250));
    setColor(theme.colors, ThemeSlot::ToolTipBackground, rgb(255, 255, 225));
    setColor(theme.colors, ThemeSlot::ToolTipBorder, rgb(120, 120, 120));
    setColor(theme.colors, ThemeSlot::ToolTipText, rgb(40, 40, 40));
    setColor(theme.colors, ThemeSlot::ScrollViewBackground, rgb(252, 252, 252));
    setColor(theme.colors, ThemeSlot::SplitterBar, rgb(238, 238, 240));
    setColor(theme.colors, ThemeSlot::SplitterHover, rgb(160, 207, 255));
    setColor(theme.colors, ThemeSlot::NativeHostBackground, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::NativeHostBorder, rgb(180, 188, 200));
    setColor(theme.colors, ThemeSlot::NativeHostText, rgb(20, 20, 20));
    setColor(theme.colors, ThemeSlot::DialogBackground, rgb(242, 242, 242));
    setColor(theme.colors, ThemeSlot::DialogTitleBackground, rgb(0, 95, 135));
    setColor(theme.colors, ThemeSlot::DialogCaptionHover, rgb(232, 232, 232));
    setColor(theme.colors, ThemeSlot::DialogCaptionPressed, rgb(212, 212, 212));
    setColor(theme.colors, ThemeSlot::DialogCloseHover, rgb(232, 17, 35));
    setColor(theme.colors, ThemeSlot::DialogClosePressed, rgb(180, 10, 25));
    setColor(theme.colors, ThemeSlot::DialogTitleText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::DialogCaptionHoverText, rgb(0, 0, 0));
    setColor(theme.colors, ThemeSlot::DialogBorder, rgb(220, 220, 220));
    setColor(theme.colors, ThemeSlot::DialogPanelBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::DialogPanelBorder, rgb(200, 200, 200));
    setColor(theme.colors, ThemeSlot::DialogAccent, rgb(0, 100, 128));
    setColor(theme.colors, ThemeSlot::DialogButtonHover, rgb(218, 236, 240));
    setColor(theme.colors, ThemeSlot::DialogPrimaryHover, rgb(0, 118, 148));
    setColor(theme.colors, ThemeSlot::DialogPrimaryPressed, rgb(0, 78, 111));
    setColor(theme.colors, ThemeSlot::DialogDisabledBackground, rgb(245, 245, 245));
    setColor(theme.colors, ThemeSlot::DialogDisabledBorder, rgb(180, 180, 180));
    setColor(theme.colors, ThemeSlot::DialogDisabledText, rgb(160, 160, 160));
    setColor(theme.colors, ThemeSlot::DialogText, rgb(20, 20, 20));
    setColor(theme.colors, ThemeSlot::MessageBackground, rgb(240, 240, 240));
    setColor(theme.colors, ThemeSlot::MessageWarningFill, rgb(244, 176, 35));
    setColor(theme.colors, ThemeSlot::MessageWarningText, rgb(80, 58, 12));
    setColor(theme.colors, ThemeSlot::MessageErrorFill, rgb(196, 37, 43));
    setColor(theme.colors, ThemeSlot::MessageInformationFill, rgb(45, 108, 190));
    // FillLight 不调用 applyDerivedColors，Message* 系列须逐一显式赋值
    setColor(theme.colors, ThemeSlot::MessageSuccessFill, rgb(60, 175, 80));
    // 对话与标签语义槽（FillLight 不跑派生映射，须逐一显式赋值）
    setColor(theme.colors, ThemeSlot::ChatUserBubbleFill, rgb(222, 235, 254));
    setColor(theme.colors, ThemeSlot::ChatUserBubbleText, rgb(23, 43, 77));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleFill, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleText, rgb(30, 30, 30));
    setColor(theme.colors, ThemeSlot::ChatMetaText, rgb(140, 140, 148));
    setColor(theme.colors, ThemeSlot::ChipFill, rgb(238, 242, 248));
    setColor(theme.colors, ThemeSlot::ChipText, rgb(50, 56, 66));
    setColor(theme.colors, ThemeSlot::ChipBorder, rgb(210, 216, 226));
    setColor(theme.colors, ThemeSlot::ImageViewerBackdrop, rgba(16, 18, 24, 214));
}

void DuiTheme::FillDark(Impl& theme)
{
    theme.colors = {rgb(80, 140, 240), rgb(100, 160, 255), rgb(60, 120, 220), rgb(100, 160, 255),
                    rgb(255, 255, 255), rgb(230, 230, 235), rgb(160, 160, 170), rgb(100, 100, 110),
                    rgb(120, 180, 255), rgb(30, 32, 38), rgb(40, 44, 52), rgb(60, 64, 72),
                    rgb(80, 84, 92), rgb(50, 60, 80), rgb(80, 140, 240), rgb(255, 255, 255),
                    rgb(90, 200, 110), rgb(255, 200, 60), rgb(255, 90, 90), rgb(140, 140, 150)};
    setColor(theme.colors, ThemeSlot::ControlBackground, rgb(48, 52, 60));
    setColor(theme.colors, ThemeSlot::ControlHover, rgb(58, 64, 74));
    setColor(theme.colors, ThemeSlot::ControlPressed, rgb(42, 46, 54));
    setColor(theme.colors, ThemeSlot::ControlDisabled, rgb(55, 58, 64));
    setColor(theme.colors, ThemeSlot::ControlBorder, rgb(88, 94, 104));
    setColor(theme.colors, ThemeSlot::SelectionBackground, rgb(50, 64, 88));
    // 文本输入框选中：与浅色主题统一使用 Windows 标准高亮蓝 + 白字
    setColor(theme.colors, ThemeSlot::TextSelectionBackground, rgb(0, 120, 215));
    setColor(theme.colors, ThemeSlot::TextSelectionText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ToggleOn, rgb(60, 180, 105));
    setColor(theme.colors, ThemeSlot::ToggleOff, rgb(80, 84, 92));
    setColor(theme.colors, ThemeSlot::Danger, rgb(225, 82, 88));
    setColor(theme.colors, ThemeSlot::DangerHover, rgb(245, 102, 108));
    setColor(theme.colors, ThemeSlot::DangerPressed, rgb(190, 58, 64));
    applyDerivedColors(theme.colors, ThemeSlot::BrandHover, ThemeSlot::SurfaceAlternateBackground);
    setColor(theme.colors, ThemeSlot::ToastBackground, rgb(50, 50, 54));
    setColor(theme.colors, ThemeSlot::ChatUserBubbleFill, rgb(50, 64, 88));
    setColor(theme.colors, ThemeSlot::ChatUserBubbleText, rgb(232, 238, 248));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleFill, rgb(48, 52, 60));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleText, rgb(230, 230, 235));
    setColor(theme.colors, ThemeSlot::ChatMetaText, rgb(140, 140, 150));
    setColor(theme.colors, ThemeSlot::ChipFill, rgb(58, 62, 70));
    setColor(theme.colors, ThemeSlot::ChipText, rgb(220, 224, 230));
    setColor(theme.colors, ThemeSlot::ChipBorder, rgb(88, 94, 104));
    setColor(theme.colors, ThemeSlot::ImageViewerBackdrop, rgba(8, 9, 12, 226));
}

void DuiTheme::FillElementLight(Impl& theme)
{
    FillLight(theme);

    setColor(theme.colors, ThemeSlot::BrandPrimary, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::BrandHover, rgb(102, 177, 255));
    setColor(theme.colors, ThemeSlot::BrandPressed, rgb(58, 142, 230));
    setColor(theme.colors, ThemeSlot::BrandBorder, rgb(51, 126, 204));
    setColor(theme.colors, ThemeSlot::TextDefault, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::TextSubtle, rgb(144, 147, 153));
    setColor(theme.colors, ThemeSlot::TextDisabled, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::TextLink, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::SurfaceBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::SurfaceAlternateBackground, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::BorderLight, rgb(235, 238, 245));
    setColor(theme.colors, ThemeSlot::BorderHeavy, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::RowHover, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::RowSelected, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::TextOnSelectedRow, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::ControlBackground, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ControlHover, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::ControlPressed, rgb(238, 242, 247));
    setColor(theme.colors, ThemeSlot::ControlDisabled, rgb(244, 244, 245));
    setColor(theme.colors, ThemeSlot::ControlBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::SelectionBackground, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::StatusOnline, rgb(103, 194, 58));
    setColor(theme.colors, ThemeSlot::StatusAway, rgb(230, 162, 60));
    setColor(theme.colors, ThemeSlot::StatusBusy, rgb(245, 108, 108));
    setColor(theme.colors, ThemeSlot::StatusOffline, rgb(144, 147, 153));
    setColor(theme.colors, ThemeSlot::ToggleOn, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::Danger, rgb(245, 108, 108));
    setColor(theme.colors, ThemeSlot::DangerHover, rgb(246, 137, 137));
    setColor(theme.colors, ThemeSlot::DangerPressed, rgb(221, 82, 77));
    applyDerivedColors(theme.colors, ThemeSlot::TextLink, ThemeSlot::SurfaceAlternateBackground);
    setColor(theme.colors, ThemeSlot::BadgeBackground, rgb(245, 108, 108));
    setColor(theme.colors, ThemeSlot::ButtonGhostHover, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::ButtonGhostPressed, rgb(217, 236, 255));
    setColor(theme.colors, ThemeSlot::ButtonOutlinedHover, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::ButtonOutlinedPressed, rgb(217, 236, 255));
    setColor(theme.colors, ThemeSlot::ButtonPrimaryHover, rgb(102, 177, 255));
    setColor(theme.colors, ThemeSlot::ButtonPrimaryPressed, rgb(58, 142, 230));
    setColor(theme.colors, ThemeSlot::ButtonText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::ButtonDisabledText, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::ButtonChoiceDisabledText, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::ButtonChoiceGhostText, rgb(96, 98, 102));
    setColor(theme.colors, ThemeSlot::ButtonChoiceBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::FieldBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::FieldDisabledBorder, rgb(228, 231, 237));
    setColor(theme.colors, ThemeSlot::FieldFocusBorder, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::FieldHoverBorder, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::FieldText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::FieldDisabledText, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::FieldPlaceholder, rgb(168, 171, 178));
    setColor(theme.colors, ThemeSlot::InputArrow, rgb(144, 147, 153));
    setColor(theme.colors, ThemeSlot::ComboArrow, rgb(144, 147, 153));
    setColor(theme.colors, ThemeSlot::ControlText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::SearchPlaceholder, rgb(168, 171, 178));
    setColor(theme.colors, ThemeSlot::SearchGlyph, rgb(144, 147, 153));
    setColor(theme.colors, ThemeSlot::SearchClear, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::PopupBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::ListSelection, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::ListSelectionText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::ListHover, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::ListText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::ListCheckboxBorder, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::ListCheckboxFill, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::ListInsertion, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::GridBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::GridHeaderBackground, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::GridLine, rgb(235, 238, 245));
    setColor(theme.colors, ThemeSlot::GridSelection, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::SpreadsheetHeaderBackground, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::SpreadsheetGridLine, rgb(235, 238, 245));
    setColor(theme.colors, ThemeSlot::SpreadsheetSelection, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::SpreadsheetActiveCell, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::TreeSelection, rgb(236, 245, 255)); // 与 MenuHighlight 一致
    setColor(theme.colors, ThemeSlot::TreeHover, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::TabHover, rgb(245, 247, 250));
    setColor(theme.colors, ThemeSlot::TabSelectedText, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::TabBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::MenuBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::MenuSeparator, rgb(235, 238, 245));
    setColor(theme.colors, ThemeSlot::MenuHighlight, rgb(236, 245, 255));
    setColor(theme.colors, ThemeSlot::MenuText, rgb(48, 49, 51));
    setColor(theme.colors, ThemeSlot::MenuDisabledText, rgb(192, 196, 204));
    setColor(theme.colors, ThemeSlot::BusyIndicator, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::ToolTipBorder, rgb(220, 223, 230));
    setColor(theme.colors, ThemeSlot::SplitterHover, rgb(102, 177, 255));
    setColor(theme.colors, ThemeSlot::DialogTitleBackground, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::DialogTitleText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::DialogAccent, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::DialogPrimaryHover, rgb(102, 177, 255));
    setColor(theme.colors, ThemeSlot::DialogPrimaryPressed, rgb(58, 142, 230));
    setColor(theme.colors, ThemeSlot::MessageWarningFill, rgb(230, 162, 60));
    setColor(theme.colors, ThemeSlot::MessageErrorFill, rgb(245, 108, 108));
    setColor(theme.colors, ThemeSlot::MessageInformationFill, rgb(64, 158, 255));
    setColor(theme.colors, ThemeSlot::ToastBackground, rgb(48, 49, 51));
}

void DuiTheme::FillHighContrast(Impl& theme)
{
    theme.colors = {rgb(255, 255, 0), rgb(255, 255, 100), rgb(200, 200, 0), rgb(255, 255, 255),
                    rgb(0, 0, 0), rgb(255, 255, 255), rgb(200, 200, 200), rgb(120, 120, 120),
                    rgb(0, 255, 255), rgb(0, 0, 0), rgb(20, 20, 20), rgb(255, 255, 255),
                    rgb(255, 255, 255), rgb(80, 80, 0), rgb(255, 255, 0), rgb(0, 0, 0),
                    rgb(0, 255, 0), rgb(255, 255, 0), rgb(255, 0, 0), rgb(180, 180, 180)};
    setColor(theme.colors, ThemeSlot::ControlBackground, rgb(0, 0, 0));
    setColor(theme.colors, ThemeSlot::ControlHover, rgb(40, 40, 0));
    setColor(theme.colors, ThemeSlot::ControlPressed, rgb(80, 80, 0));
    setColor(theme.colors, ThemeSlot::ControlDisabled, rgb(45, 45, 45));
    setColor(theme.colors, ThemeSlot::ControlBorder, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::SelectionBackground, rgb(80, 80, 0));
    // 高对比度主题沿用自身选中配色（深底黄字），不采用标准高亮蓝
    setColor(theme.colors, ThemeSlot::TextSelectionBackground, rgb(80, 80, 0));
    setColor(theme.colors, ThemeSlot::TextSelectionText, rgb(255, 255, 0));
    setColor(theme.colors, ThemeSlot::ToggleOn, rgb(0, 255, 0));
    setColor(theme.colors, ThemeSlot::ToggleOff, rgb(80, 80, 80));
    setColor(theme.colors, ThemeSlot::Danger, rgb(255, 0, 0));
    setColor(theme.colors, ThemeSlot::DangerHover, rgb(255, 80, 80));
    setColor(theme.colors, ThemeSlot::DangerPressed, rgb(180, 0, 0));
    applyDerivedColors(theme.colors, ThemeSlot::TextLink, ThemeSlot::SurfaceAlternateBackground);
    setColor(theme.colors, ThemeSlot::ChatUserBubbleFill, rgb(80, 80, 0));
    setColor(theme.colors, ThemeSlot::ChatUserBubbleText, rgb(255, 255, 0));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleFill, rgb(0, 0, 0));
    setColor(theme.colors, ThemeSlot::ChatAssistantBubbleText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ChatMetaText, rgb(200, 200, 200));
    setColor(theme.colors, ThemeSlot::ChipFill, rgb(0, 0, 0));
    setColor(theme.colors, ThemeSlot::ChipText, rgb(255, 255, 255));
    setColor(theme.colors, ThemeSlot::ChipBorder, rgb(255, 255, 255));
    // 高对比度：背板不透明，避免底下的内容干扰
    setColor(theme.colors, ThemeSlot::ImageViewerBackdrop, rgba(0, 0, 0, 255));
}

void DuiTheme::Notify(Impl& theme)
{
    ++theme.version;
    const auto subscriptions = theme.subscriptionStore->subscriptions;
    for (const auto& subscription : subscriptions)
    {
        if (subscription.callback)
            subscription.callback();
    }
}
DuiTheme::DuiTheme() : theme_(std::make_unique<Impl>()) { FillLight(*theme_); }
DuiTheme::~DuiTheme() = default;
DuiTheme::DuiTheme(DuiTheme&&) noexcept = default;
DuiTheme& DuiTheme::operator=(DuiTheme&&) noexcept = default;
Color DuiTheme::Get(ThemeSlot slot) const { return theme_->colors[indexOf(slot)]; }
void DuiTheme::Set(ThemeSlot slot, Color color) {
    if (Get(slot) == color) return;
    theme_->colors[indexOf(slot)] = color;
    Notify(*theme_);
}
Color DuiTheme::GetStateColor(ThemeSlot base, VisualState state) const {
    if (state == VisualState::Disabled) return Get(ThemeSlot::TextDisabled);
    if (base == ThemeSlot::BrandPrimary) {
        if (state == VisualState::Hover) return Get(ThemeSlot::BrandHover);
        if (state == VisualState::Active) return Get(ThemeSlot::BrandPressed);
        if (state == VisualState::Focused) return Get(ThemeSlot::BrandBorder);
    }
    if (base == ThemeSlot::TextDefault && state == VisualState::Hover) return Get(ThemeSlot::TextLink);
    return Get(base);
}
void DuiTheme::ApplyPreset(ThemePreset preset) {
    if (preset == ThemePreset::Dark) FillDark(*theme_);
    else if (preset == ThemePreset::HighContrast) FillHighContrast(*theme_);
    else if (preset == ThemePreset::ElementLight) FillElementLight(*theme_);
    else FillLight(*theme_);
    Notify(*theme_);
}
unsigned DuiTheme::Version() const { return theme_->version; }
DuiSubscription DuiTheme::SubscribeScoped(std::function<void()> callback)
{
    if (!callback)
        return {};
    const auto store = theme_->subscriptionStore;
    const std::size_t token = store->nextToken++;
    DuiSubscription subscription([weakStore = std::weak_ptr<Impl::SubscriptionStore>{store}, token]
    {
        if (const auto lockedStore = weakStore.lock())
        {
            std::erase_if(lockedStore->subscriptions, [token](const Impl::SubscriptionStore::Subscription& subscription)
            {
                return subscription.token == token;
            });
        }
    });
    store->subscriptions.push_back({token, std::move(callback)});
    return subscription;
}
std::size_t DuiTheme::Subscribe(std::function<void()> callback) {
    if (!callback) return 0;
    const std::size_t token = theme_->subscriptionStore->nextToken++;
    theme_->subscriptionStore->subscriptions.push_back({token, std::move(callback)});
    return token;
}
void DuiTheme::Unsubscribe(std::size_t token) {
    std::erase_if(theme_->subscriptionStore->subscriptions,
                  [token](const Impl::SubscriptionStore::Subscription& subscription)
                  {
                      return subscription.token == token;
                  });
}
void DuiTheme::SetDefaultFontFamily(std::string family) {
    if (theme_->fontFamily == family) return;
    theme_->fontFamily = std::move(family);
    Notify(*theme_);
}
const std::string& DuiTheme::DefaultFontFamily() const { return theme_->fontFamily; }
void DuiTheme::SetDefaultFontPointSize(int points) {
    points = std::clamp(points, 6, 96);
    if (theme_->fontPointSize == points) return;
    theme_->fontPointSize = points;
    Notify(*theme_);
}
int DuiTheme::DefaultFontPointSize() const { return theme_->fontPointSize; }

void DuiTheme::SetFocusRingVisible(bool visible) {
    if (theme_->focusRingVisible == visible) return;
    theme_->focusRingVisible = visible;
    Notify(*theme_);
}
bool DuiTheme::FocusRingVisible() const { return theme_->focusRingVisible; }

} // namespace ysDui::core
