/**
 * 文件名：DuiXmlBuilder.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的 XML 控件树构建器。
 */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/ui/DuiFrameHost.hpp"

namespace ysDui::render {
class DuiImage;
}

namespace ysDui::controls {

/**
 * 顶层窗口 XML 的构建结果。
 * 用途：将窗口配置和客户区控件树作为一个平台无关的所有权单元交给宿主工厂。
 */
struct DuiFrameXmlDocument final
{
    ui::DuiFrameOptions options;                 // 顶层窗口的无平台配置。
    std::unique_ptr<core::Control> content;      // 顶层窗口客户区的根控件。
};

/** XML 控件树构建失败类别。 */
enum class DuiXmlErrorCode
{
    None,                   // 没有错误。
    ParseError,             // XML 语法无效。
    MissingRootElement,     // 文档不包含根元素。
    UnknownElement,         // 元素没有注册控件创建器。
    ControlCreationFailed,  // 已注册创建器未返回控件。
    InvalidFrameRoot,       // 顶层窗口根元素不是 frame-window。
    InvalidFrameContent,    // 顶层窗口客户区根元素数量不是一个。
    InvalidAttribute        // 属性值无法按目标类型解析。
};

/** XML 控件树构建错误，位置使用 UTF-8 源文本中的字节偏移和 1 基行列。 */
struct DuiXmlError final
{
    DuiXmlErrorCode code{DuiXmlErrorCode::None};
    std::string message;
    std::string element;
    std::string attribute;
    std::size_t offset{};
    std::size_t line{1};
    std::size_t column{1};
};

/** 普通控件树的结构化构建结果。 */
struct DuiXmlControlBuildResult final
{
    std::unique_ptr<core::Control> control;
    std::optional<DuiXmlError> error;

    [[nodiscard]] explicit operator bool() const noexcept { return control != nullptr; }
};

/** 顶层窗口 XML 的结构化构建结果。 */
struct DuiFrameXmlBuildResult final
{
    DuiFrameXmlDocument document;
    std::optional<DuiXmlError> error;

    [[nodiscard]] explicit operator bool() const noexcept { return document.content != nullptr; }
};

/**
 * XML 控件树构建器。
 * 用途：将 UTF-8 XML 的元素和属性映射为控件树，不暴露 XML 解析器或平台资源类型。
 */
class DuiXmlBuilder final
{
public:
    /** 自定义元素的控件创建函数。 */
    using ControlCreator = std::function<std::unique_ptr<core::Control>()>;
    /** 自定义元素的属性赋值函数。 */
    using PropertyApplier = std::function<bool(core::Control&, std::string_view, std::string_view)>;
    /** 逻辑资源路径解析函数。 */
    using ResourceResolver = std::function<std::string(std::string_view)>;
    /** 图像资源解析函数。 */
    using ImageResolver = std::function<std::shared_ptr<const render::DuiImage>(std::string_view)>;

    DuiXmlBuilder();
    ~DuiXmlBuilder();
    DuiXmlBuilder(const DuiXmlBuilder&) = delete;
    DuiXmlBuilder& operator=(const DuiXmlBuilder&) = delete;
    DuiXmlBuilder(DuiXmlBuilder&&) noexcept;
    DuiXmlBuilder& operator=(DuiXmlBuilder&&) noexcept;

    /**
     * 注册自定义 XML 元素。
     * @param element 不区分大小写的元素名。
     * @param creator 创建控件的函数。
     * @param applier 可选的属性赋值函数。
     */
    void RegisterControl(std::string element, ControlCreator creator, PropertyApplier applier = {});

    /**
     * 设置逻辑资源路径解析函数。
     * @param resolver 接收 XML 内的 UTF-8 逻辑路径并返回调用方定义的路径。
     */
    void SetResourceResolver(ResourceResolver resolver);
    void SetImageResolver(ImageResolver resolver);

    /**
     * 解析逻辑资源路径。
     * @param path XML 属性中的 UTF-8 逻辑路径。
     * @return 已解析路径；未设置解析函数时原样返回。
     */
    [[nodiscard]] std::string ResolveResourcePath(std::string_view path) const;

    /**
     * 构建 XML 控件树并返回结构化错误。
     * @param xml UTF-8 XML 文本。
     * @return 成功时包含根控件，失败时包含错误类别、上下文和源位置。
     */
    [[nodiscard]] DuiXmlControlBuildResult BuildWithResult(std::string_view xml) const;

    /**
     * 构建 XML 控件树。
     * @param xml UTF-8 XML 文本。
     * @return 成功时返回根控件；XML 无效或根元素未注册时返回空指针。
     */
    [[deprecated("Use BuildWithResult() to preserve XML diagnostics")]]
    [[nodiscard]] std::unique_ptr<core::Control> Build(std::string_view xml) const;

    /**
     * 构建顶层窗口 XML 并返回结构化错误。
     * @param xml UTF-8 XML 文本，根元素必须为 frame-window 并包含一个客户区根控件。
     * @return 成功时包含窗口文档，失败时包含错误类别、上下文和源位置。
     */
    [[nodiscard]] DuiFrameXmlBuildResult BuildFrameWithResult(std::string_view xml) const;

    /**
     * 构建顶层窗口 XML。
     * @param xml UTF-8 XML 文本，根元素必须为 frame-window 并包含一个客户区根控件。
     * @return 成功时返回窗口配置和客户区控件树，否则返回空值。
     */
    [[deprecated("Use BuildFrameWithResult() to preserve XML diagnostics")]]
    [[nodiscard]] std::optional<DuiFrameXmlDocument> BuildFrame(std::string_view xml) const;

private:
    class Impl;
    std::unique_ptr<Impl> builder_;
};

} // namespace ysDui::controls
