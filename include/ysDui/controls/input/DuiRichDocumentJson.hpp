/**
 * 文件名：DuiRichDocumentJson.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关富文档 JSON 持久化协议。
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ysDui/controls/input/DuiRichDocument.hpp"

namespace ysDui::controls::input {

/**
 * 富文档 JSON 编解码器。
 * 用途：在不暴露 RTF、平台专有对象或平台类型的前提下持久化富文档值模型。
 */
class DuiRichDocumentJson final
{
public:
    /**
     * 将富文档编码为 UTF-8 JSON。
     * @param document 待持久化的富文档值。
     * @return 版本化 JSON 文本；图像仅保存资源包条目引用。
     */
    [[nodiscard]] static std::string Serialize(const DuiRichDocument& document);

    /**
     * 从 UTF-8 JSON 恢复富文档。
     * @param json 版本化 JSON 文本。
     * @return 格式和字段均有效时返回文档，否则返回空值。
     */
    [[nodiscard]] static std::optional<DuiRichDocument> Deserialize(std::string_view json);
};

} // namespace ysDui::controls::input
