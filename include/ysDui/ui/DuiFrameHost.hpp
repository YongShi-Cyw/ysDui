/**
 * 文件名：DuiFrameHost.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明跨平台顶层窗口与自定义标题栏宿主接口。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiNinePatch.hpp"
#include "ysDui/ui/DuiHostRef.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"

namespace ysDui::core {
class DuiTheme;
}

namespace ysDui::ui {

/**
 * 标题栏按钮的标准行为。
 */
enum class DuiFrameCaptionAction
{
    Minimize,        // 最小化顶层窗口。
    ToggleMaximize,  // 在最大化和还原间切换。
    Close,           // 请求关闭顶层窗口。
    Custom           // 通知调用方处理自定义按钮。
};

/**
 * 自定义标题栏按钮描述。
 */
struct DuiFrameCaptionButton final
{
    int id{};                                            // 自定义按钮的稳定标识。
    DuiFrameCaptionAction action{DuiFrameCaptionAction::Custom}; // 点击后的标准行为。
    std::shared_ptr<const render::DuiImage> image;       // 可选的已解码图标。
    std::string glyph;                                  // 无图标时绘制的短文本（如 "Dock"）。
    std::string tooltip;                               // 平台可选地显示的辅助文本。
    bool left{};                                         // true 时放在标题文字左侧。
};

/**
 * 顶层窗口和自定义标题栏的无平台配置。
 */
struct DuiFrameOptions final
{
    std::string title;
    core::Point position;
    core::Size size{800, 600};
    core::Size minimumSize{200, 150};
    core::Size maximumSize;
    int titleBarHeight{36};
    int resizeBorder{8};
    core::Color titleTextColor{40, 40, 40, 255};
    core::Color titleBarColor{245, 246, 250, 255};
    core::Color captionGlyphColor{70, 70, 70, 255};
    /** 客户区底色；alpha 为 0 时不绘制（由平台窗口类背景呈现）。 */
    core::Color backgroundColor{255, 255, 255, 255};
    /** 窗口轮廓线颜色；alpha 为 0 时不绘制。 */
    core::Color borderColor{0, 0, 0, 0};
    /** 标题栏前置图标（画在标题文字之前）；空指针时不绘制。 */
    std::shared_ptr<const render::DuiImage> icon;
    /**
     * 主题：非空时标题栏、文字、轮廓、按钮反馈色改用与 `DuiDialog` 相同的主题槽
     * （DialogTitleBackground / DialogTitleText / DialogBorder / DialogCaptionHover 等），
     * 上面几个显式颜色字段退化为回退值。传入的主题须比窗口活得更久。
     */
    const core::DuiTheme* theme{};
    std::shared_ptr<const render::DuiImage> backgroundImage;
    render::DuiNinePatchInsets backgroundSourceInsets;
    render::DuiNinePatchInsets backgroundDestinationInsets;
    std::vector<DuiFrameCaptionButton> captionButtons;
    HostRef owner;
    bool resizable{true};
    bool titleBarTransparent{};
    bool preferSquareCorners{true};
    bool showMinimize{true};
    bool showMaximize{true};
    bool showClose{true};
};

/**
 * 跨平台顶层窗口宿主。
 * 用途：承载一棵控件树并提供平台无关的标题栏、尺寸和关闭行为。
 */
class IFrameHost
{
public:
    using LayoutHandler = IPopupHost::LayoutHandler;
    using PaintHandler = IPopupHost::PaintHandler;

    virtual ~IFrameHost() = default;

    /**
     * 创建并显示顶层窗口。
     * @param options 窗口和标题栏配置。
     * @param content 窗口客户区控件树。
     * @param layout 客户区布局回调。
     * @param paint 客户区绘制回调。
     * @return 创建成功时返回 true。
     */
    virtual bool Show(const DuiFrameOptions& options, std::unique_ptr<core::Control> content,
                      LayoutHandler layout, PaintHandler paint) = 0;

    /**
     * 更新已显示窗口的配置。
     * @param options 新的窗口和标题栏配置。
     */
    virtual void SetOptions(const DuiFrameOptions& options) = 0;

    /**
     * 替换客户区控件树。
     * @param content 新客户区控件树。
     * @param layout 客户区布局回调。
     * @param paint 客户区绘制回调。
     */
    virtual void SetContent(std::unique_ptr<core::Control> content, LayoutHandler layout, PaintHandler paint) = 0;

    /** 关闭并释放窗口资源。 */
    virtual void Close() = 0;
    /**
     * 摘出客户区控件树并交出所有权，窗口随即变为空内容。
     * 用途：内容需要在宿主之间搬迁时（把悬浮窗里的窗格收回停靠区）先摘出再挂到新宿主。
     * 后端不实现时保持默认（返回空指针），调用方据此判定"不支持收回"。
     * 说明：摘出后应尽快 `Close()`；窗口的布局/绘制回调仍指向原内容。
     * @return 原客户区控件树；无内容或不支持时返回空指针
     */
    [[nodiscard]] virtual std::unique_ptr<core::Control> DetachContent() { return {}; }
    /** 请求平台关闭窗口。 */
    virtual void RequestClose() = 0;
    /** @return 窗口是否处于可见状态。 */
    [[nodiscard]] virtual bool Visible() const = 0;
    /** @return 当前顶层宿主的弱引用；宿主关闭后引用失效。 */
    [[nodiscard]] virtual HostRef Reference() const = 0;
    /** 设置自定义标题栏按钮点击回调。 */
    virtual void SetCaptionButtonHandler(std::function<void(int)> handler) = 0;
    /** 设置窗口关闭回调。 */
    virtual void SetClosedHandler(std::function<void()> handler) = 0;
};

} // namespace ysDui::ui
