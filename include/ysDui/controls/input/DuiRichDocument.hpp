/**
 * 文件名：DuiRichDocument.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的富文本文档值模型。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/ui/DuiRichTextInput.hpp"

namespace ysDui::controls::input {

/**
 * 富文本文档块类型。
 */
enum class DuiRichDocumentBlockKind {
    Text,      // 带格式和链接的 UTF-16 文本段。
    Quote,     // 发送者和正文构成的引用内容。
    FileCard,  // 文件名和字节数构成的文件卡片。
    Image,     // 由资源包条目引用的图像。
};

/**
 * 带字符格式和可选链接的 UTF-16 文本段。
 */
struct DuiRichTextRun final {
    std::string text;
    ui::DuiRichTextFormat format;
    std::string link;
    bool operator==(const DuiRichTextRun&) const = default;
};

/**
 * 平台无关的富文本文档块。
 */
struct DuiRichDocumentBlock final {
    DuiRichDocumentBlockKind kind{DuiRichDocumentBlockKind::Text};
    std::vector<DuiRichTextRun> runs;
    std::string sender;
    std::string body;
    std::string fileName;
    std::uint64_t fileSize{};
    std::string imageResourceEntry;
    core::Size imageSize;
    bool operator==(const DuiRichDocumentBlock&) const = default;
};

/**
 * 平台无关的富文本文档。
 *
 * 该类保存可移植的文本、引用和文件卡片内容；不保存后端专有富文本或图像负载。
 */
class DuiRichDocument final {
public:
    /** 创建空文档。 */
    DuiRichDocument();
    /** 销毁文档。 */
    ~DuiRichDocument();
    /** 复制另一份文档的全部块。 */
    DuiRichDocument(const DuiRichDocument& other);
    /** 以另一份文档的内容替换当前文档。 */
    DuiRichDocument& operator=(const DuiRichDocument& other);
    /** 转移文档所有权。 */
    DuiRichDocument(DuiRichDocument&&) noexcept;
    /** 转移另一份文档的内容。 */
    DuiRichDocument& operator=(DuiRichDocument&&) noexcept;

    /** 清空全部文档块。 */
    void Clear();
    /**
     * 添加文本块。
     * @param runs 按显示顺序排列的文本段。
     */
    void AddTextBlock(std::vector<DuiRichTextRun> runs);
    /**
     * 添加引用块。
     * @param sender 引用内容的发送者。
     * @param body 引用正文。
     */
    void AddQuoteBlock(std::string sender, std::string body);
    /**
     * 添加文件卡片。
     * @param fileName 文件显示名。
     * @param fileSize 文件字节数。
     */
    void AddFileCard(std::string fileName, std::uint64_t fileSize);
    /**
     * 添加由资源包条目引用的图像块。
     * @param resourceEntry 图像在资源包中的 UTF-8 条目名。
     * @param imageSize 图像的逻辑像素尺寸；零尺寸表示由解码器确定。
     */
    void AddImageBlock(std::string resourceEntry, core::Size imageSize = {});
    /**
     * 返回按插入顺序保存的文档块。
     * @return 不可修改的文档块列表。
     */
    [[nodiscard]] const std::vector<DuiRichDocumentBlock>& Blocks() const;
    /**
     * 判断文档是否未包含任何块。
     * @return 文档为空时返回 true。
     */
    [[nodiscard]] bool Empty() const;

private:
    class Impl;
    std::unique_ptr<Impl> document_;
};

} // namespace ysDui::controls::input
