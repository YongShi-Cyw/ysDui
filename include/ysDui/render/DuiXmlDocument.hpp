/**
 * 文件名：DuiXmlDocument.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的轻量 XML 文档读取接口。
 */
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ysDui::render {

/**
 * 平台无关的只读 XML 文档。
 * 用途：为资源清单和控件配置提供 UTF-8 XML 的基础查询，不暴露解析器节点类型。
 */
class DuiXmlDocument final
{
public:
    DuiXmlDocument();
    ~DuiXmlDocument();
    DuiXmlDocument(const DuiXmlDocument&) = delete;
    DuiXmlDocument& operator=(const DuiXmlDocument&) = delete;
    DuiXmlDocument(DuiXmlDocument&&) noexcept;
    DuiXmlDocument& operator=(DuiXmlDocument&&) noexcept;

    /**
     * 加载 UTF-8 编码的 XML 文本。
     * @param xml UTF-8 XML 文本。
     * @return 成功时返回 true；失败时保留当前已加载的文档。
     */
    [[nodiscard]] bool Load(std::string_view xml);

    /**
     * 判断是否已加载有效的 XML 根元素。
     * @return 存在根元素时返回 true。
     */
    [[nodiscard]] bool Loaded() const;

    /**
     * 获取根元素名称。
     * @return 根元素名称；未加载时返回空字符串。
     */
    [[nodiscard]] std::string RootName() const;

    /**
     * 获取根元素属性值。
     * @param name 属性名称。
     * @return 属性存在时返回 UTF-8 值，否则返回空值。
     */
    [[nodiscard]] std::optional<std::string> RootAttribute(std::string_view name) const;

    /**
     * 获取第一个同名直接子元素的文本。
     * @param name 子元素名称。
     * @return 子元素存在时返回 UTF-8 文本，否则返回空值。
     */
    [[nodiscard]] std::optional<std::string> ChildText(std::string_view name) const;

    /**
     * 获取第一个同名直接子元素的属性值。
     * @param childName 子元素名称。
     * @param attributeName 属性名称。
     * @return 属性存在时返回 UTF-8 值，否则返回空值。
     */
    [[nodiscard]] std::optional<std::string> ChildAttribute(std::string_view childName,
                                                             std::string_view attributeName) const;

private:
    class Impl;
    std::unique_ptr<Impl> document_;
};

} // namespace ysDui::render
