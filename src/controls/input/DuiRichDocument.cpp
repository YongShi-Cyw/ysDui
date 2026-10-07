/**
 * 文件名：DuiRichDocument.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现平台无关富文本文档的值语义和块管理。
 */
#include "ysDui/controls/input/DuiRichDocument.hpp"

#include <algorithm>
#include <utility>

namespace ysDui::controls::input {

class DuiRichDocument::Impl {
public:
    std::vector<DuiRichDocumentBlock> blocks;
};

DuiRichDocument::DuiRichDocument() : document_(std::make_unique<Impl>()) {}
DuiRichDocument::~DuiRichDocument() = default;
DuiRichDocument::DuiRichDocument(const DuiRichDocument& other) : document_(std::make_unique<Impl>(*other.document_)) {}
DuiRichDocument& DuiRichDocument::operator=(const DuiRichDocument& other)
{
    if (this != &other)
        document_ = std::make_unique<Impl>(*other.document_);
    return *this;
}
DuiRichDocument::DuiRichDocument(DuiRichDocument&&) noexcept = default;
DuiRichDocument& DuiRichDocument::operator=(DuiRichDocument&&) noexcept = default;
void DuiRichDocument::Clear() { document_->blocks.clear(); }
void DuiRichDocument::AddTextBlock(std::vector<DuiRichTextRun> runs) { document_->blocks.push_back({DuiRichDocumentBlockKind::Text, std::move(runs), {}, {}, {}, 0, {}, {}}); }
void DuiRichDocument::AddQuoteBlock(std::string sender, std::string body) { document_->blocks.push_back({DuiRichDocumentBlockKind::Quote, {}, std::move(sender), std::move(body), {}, 0, {}, {}}); }
void DuiRichDocument::AddFileCard(std::string fileName, std::uint64_t fileSize) { document_->blocks.push_back({DuiRichDocumentBlockKind::FileCard, {}, {}, {}, std::move(fileName), fileSize, {}, {}}); }
void DuiRichDocument::AddImageBlock(std::string resourceEntry, core::Size imageSize)
{
    if (resourceEntry.empty())
        return;
    DuiRichDocumentBlock block;
    block.kind = DuiRichDocumentBlockKind::Image;
    block.imageResourceEntry = std::move(resourceEntry);
    block.imageSize = {(std::max)(0, imageSize.width), (std::max)(0, imageSize.height)};
    document_->blocks.push_back(std::move(block));
}
const std::vector<DuiRichDocumentBlock>& DuiRichDocument::Blocks() const { return document_->blocks; }
bool DuiRichDocument::Empty() const { return document_->blocks.empty(); }

} // namespace ysDui::controls::input
