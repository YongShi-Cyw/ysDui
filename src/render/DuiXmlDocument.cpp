/**
 * 文件名：DuiXmlDocument.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：使用 pugixml 实现平台无关 XML 文档读取。
 */
#include "ysDui/render/DuiXmlDocument.hpp"

#include "pugixml.hpp"

namespace ysDui::render {

class DuiXmlDocument::Impl final
{
public:
    pugi::xml_document document;
};

namespace {

pugi::xml_node Child(pugi::xml_node root, std::string_view name)
{
    return root.child(std::string(name).c_str());
}

std::optional<std::string> Attribute(pugi::xml_node node, std::string_view name)
{
    const pugi::xml_attribute attribute = node.attribute(std::string(name).c_str());
    if (!attribute)
        return std::nullopt;
    return std::string(attribute.value());
}

} // namespace

DuiXmlDocument::DuiXmlDocument() : document_(std::make_unique<Impl>()) {}
DuiXmlDocument::~DuiXmlDocument() = default;
DuiXmlDocument::DuiXmlDocument(DuiXmlDocument&&) noexcept = default;
DuiXmlDocument& DuiXmlDocument::operator=(DuiXmlDocument&&) noexcept = default;

bool DuiXmlDocument::Load(std::string_view xml)
{
    if (xml.empty())
        return false;

    pugi::xml_document candidate;
    const pugi::xml_parse_result result = candidate.load_buffer(
        xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
    if (!result || !candidate.document_element())
        return false;
    document_->document.reset(candidate);
    return true;
}

bool DuiXmlDocument::Loaded() const
{
    return static_cast<bool>(document_->document.document_element());
}

std::string DuiXmlDocument::RootName() const
{
    return document_->document.document_element().name();
}

std::optional<std::string> DuiXmlDocument::RootAttribute(std::string_view name) const
{
    return Attribute(document_->document.document_element(), name);
}

std::optional<std::string> DuiXmlDocument::ChildText(std::string_view name) const
{
    const pugi::xml_node child = Child(document_->document.document_element(), name);
    if (!child)
        return std::nullopt;
    return std::string(child.text().as_string());
}

std::optional<std::string> DuiXmlDocument::ChildAttribute(std::string_view childName,
                                                           std::string_view attributeName) const
{
    return Attribute(Child(document_->document.document_element(), childName), attributeName);
}

} // namespace ysDui::render
