#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/core/DuiSubscription.hpp"
#include "ysDui/core/DuiVisualState.hpp"

namespace ysDui::core {

enum class ThemeSlot {
    BrandPrimary,
    BrandHover,
    BrandPressed,
    BrandBorder,
    TextOnPrimary,
    TextDefault,
    TextSubtle,
    TextDisabled,
    TextLink,
    SurfaceBackground,
    SurfaceAlternateBackground,
    BorderLight,
    BorderHeavy,
    RowHover,
    RowSelected,
    TextOnSelectedRow,
    StatusOnline,
    StatusAway,
    StatusBusy,
    StatusOffline,
    ControlBackground,
    ControlHover,
    ControlPressed,
    ControlDisabled,
    ControlBorder,
    SelectionBackground,
    ToggleOn,
    ToggleOff,
    SwitchKnobBorder,
    Danger,
    DangerHover,
    DangerPressed,
    BadgeBackground,
    ButtonGhostHover,
    ButtonGhostPressed,
    ButtonOutlinedHover,
    ButtonOutlinedPressed,
    ButtonPrimaryHover,
    ButtonPrimaryPressed,
    ButtonText,
    ButtonDisabledText,
    ButtonChoiceDisabledText,
    ButtonChoiceGhostText,
    ButtonChoiceBorder,
    LinkVisited,
    BreadcrumbText,
    BreadcrumbSeparator,
    PanelHeaderBackground,
    PanelBorder,
    PanelText,
    PanelGlyph,
    GroupBorder,
    GroupText,
    SegmentedBackground,
    SegmentedText,
    StatusBackground,
    ToastBackground,
    ToolbarBackground,
    ProgressTrack,
    ProgressText,
    StatusText,
    AvatarStatusOnline,
    AvatarStatusAway,
    AvatarStatusBusy,
    AvatarStatusOffline,
    FieldDisabledBackground,
    FieldPlainDisabledBackground,
    FieldBorder,
    FieldDisabledBorder,
    FieldFocusBorder,
    FieldHoverBorder,
    FieldText,
    FieldDisabledText,
    FieldPlaceholder,
    SpinText,
    SpinDisabledText,
    SpinButtonHover,
    SpinButtonPressed,
    InputArrow,
    ComboArrow,
    ControlText,
    SearchPlaceholder,
    SearchGlyph,
    SearchClear,
    InputButtonBackground,
    InputButtonDisabledBackground,
    PopupBorder,
    ColorGridBorder,
    ColorPickerArrow,
    DateTimeArrow,
    SliderTrack,
    SliderDisabledThumb,
    SliderBorder,
    SliderDisabledBorder,
    ScrollTrack,
    ScrollThumb,
    ListBackground,
    ListSelection,
    ListSelectionText,
    ListHover,
    ListText,
    ListCheckboxBorder,
    ListCheckboxFill,
    ListInsertion,
    GridBackground,
    GridBorder,
    GridHeaderBackground,
    GridLine,
    GridSelection,
    SpreadsheetBackground,
    SpreadsheetHeaderBackground,
    SpreadsheetGridLine,
    SpreadsheetSelection,
    SpreadsheetActiveCell,
    TreeText,
    TreeSubtitle,
    TreeSelection,
    TreeZebra,
    TreeHover,
    TreeProgressTrack,
    PropertyGroupBackground,
    TabBackground,
    TabIdle,
    TabHover,
    TabSelected,
    TabText,
    TabSelectedText,
    TabBorder,
    TabCloseHoverBackground,
    TabCloseHoverStroke,
    TabGlyph,
    TabDropdownHover,
    TabScrollArrow,
    TabScrollArrowDisabled,
    MenuBackground,
    MenuBorder,
    MenuSeparator,
    MenuHighlight,
    MenuText,
    MenuDisabledText,
    MenuBarActive,
    MenuBarHover,
    MenuBarText,
    MenuBarDisabledText,
    BusyTrack,
    BusyIndicator,
    EmojiText,
    EmojiHover,
    EmojiPressed,
    ToolTipBackground,
    ToolTipBorder,
    ToolTipText,
    ScrollViewBackground,
    SplitterBar,
    SplitterHover,
    NativeHostBackground,
    NativeHostBorder,
    NativeHostText,
    DialogBackground,
    DialogTitleBackground,
    DialogCaptionHover,
    DialogCaptionPressed,
    DialogCloseHover,
    DialogClosePressed,
    DialogTitleText,
    DialogCaptionHoverText,
    DialogBorder,
    DialogPanelBackground,
    DialogPanelBorder,
    DialogAccent,
    DialogButtonHover,
    DialogPrimaryHover,
    DialogPrimaryPressed,
    DialogDisabledBackground,
    DialogDisabledBorder,
    DialogDisabledText,
    DialogText,
    MessageBackground,
    MessageWarningFill,
    MessageWarningText,
    MessageErrorFill,
    MessageInformationFill,
    /** 成功态强调色（DuiInfoBar 的 Success 级别）。 */
    MessageSuccessFill,
    /**
     * 文本输入框（EditHost/SearchBox/SpinBox 等）的鼠标选中背景色。
     * 独立于列表/表格/树的行选中色，便于输入场景单独采用标准高亮色。
     */
    TextSelectionBackground,
    /** 文本输入框选中文字颜色；须与 TextSelectionBackground 保持对比度。 */
    TextSelectionText,
    /** 用户消息气泡底色。 */
    ChatUserBubbleFill,
    /**
     * 用户消息气泡文字颜色。
     * 说明：气泡**不会**改写内容控件的文字颜色（内容自带主题），因此浅色主题下该槽取浅底深字，
     *       确保内容按默认文字色渲染时仍可读；需要深底白字时请自行设置内容控件的样式/外观。
     */
    ChatUserBubbleText,
    /** 助手消息气泡底色。 */
    ChatAssistantBubbleFill,
    /** 助手消息气泡文字颜色；须与 ChatAssistantBubbleFill 保持对比度。 */
    ChatAssistantBubbleText,
    /** 消息元信息（发送者、时间戳、状态）文字颜色。 */
    ChatMetaText,
    /** 标签（Chip）底色。 */
    ChipFill,
    /** 标签文字颜色。 */
    ChipText,
    /** 标签描边颜色。 */
    ChipBorder,
    /**
     * 图片查看器（灯箱）的背板颜色。
     * 应为**带透明度的深色**：压在内容之上形成"聚焦"效果；若取浅色会与页面同色而看不出层。
     */
    ImageViewerBackdrop,
};

enum class ThemePreset {
    Light,
    Dark,
    HighContrast,
    ElementLight,
};

class DuiTheme final {
public:
    DuiTheme();
    ~DuiTheme();
    DuiTheme(const DuiTheme&) = delete;
    DuiTheme& operator=(const DuiTheme&) = delete;
    DuiTheme(DuiTheme&&) noexcept;
    DuiTheme& operator=(DuiTheme&&) noexcept;

    [[nodiscard]] Color Get(ThemeSlot slot) const;
    void Set(ThemeSlot slot, Color color);
    [[nodiscard]] Color GetStateColor(ThemeSlot base, VisualState state) const;
    void ApplyPreset(ThemePreset preset);
    [[nodiscard]] unsigned Version() const;

    /**
     * 订阅主题变更，并由返回句柄自动管理订阅生命周期。
     * @param callback 主题颜色或默认字体变更后调用的函数。
     * @return 移动式订阅句柄；空回调返回空句柄。
     */
    [[nodiscard]] DuiSubscription SubscribeScoped(std::function<void()> callback);

    [[deprecated("Use SubscribeScoped()")]] std::size_t Subscribe(std::function<void()> callback);
    [[deprecated("Use DuiSubscription::Reset()")]] void Unsubscribe(std::size_t token);
    void SetDefaultFontFamily(std::string family);
    [[nodiscard]] const std::string& DefaultFontFamily() const;
    void SetDefaultFontPointSize(int points);
    [[nodiscard]] int DefaultFontPointSize() const;

    /**
     * 设置获得焦点时是否绘制焦点线框。
     * 默认关闭：键盘操作场景才需要线框，鼠标使用时外框常与控件自身描边叠加显脏。
     * 只影响 `render::DrawThemedFocusRing` 画的线框；输入类控件用来表示聚焦的边框颜色不受影响。
     * @param visible true 时按主题色描一圈焦点线框
     */
    void SetFocusRingVisible(bool visible);
    /** @return 是否绘制焦点线框。 */
    [[nodiscard]] bool FocusRingVisible() const;

private:
    class Impl;
    static void FillLight(Impl& theme);
    static void FillDark(Impl& theme);
    static void FillHighContrast(Impl& theme);
    static void FillElementLight(Impl& theme);
    static void Notify(Impl& theme);
    std::unique_ptr<Impl> theme_;
};

} // namespace ysDui::core
