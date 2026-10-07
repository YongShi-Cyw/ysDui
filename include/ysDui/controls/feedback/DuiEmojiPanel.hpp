#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"
#include "ysDui/render/DuiText.hpp"

namespace ysDui::controls::feedback {

class DuiEmojiPanel final : public core::Control, public render::DuiRenderable {
public:
    DuiEmojiPanel();
    ~DuiEmojiPanel() override;
    DuiEmojiPanel(const DuiEmojiPanel&) = delete;
    DuiEmojiPanel& operator=(const DuiEmojiPanel&) = delete;
    DuiEmojiPanel(DuiEmojiPanel&&) noexcept;
    DuiEmojiPanel& operator=(DuiEmojiPanel&&) noexcept;

    void AddEmoji(std::string sequence, std::string tooltip = {});
    void AddEmojiImage(std::string sequence, std::shared_ptr<const render::DuiImage> image,
                       std::string tooltip = {});
    void AddEmojiSet(const std::vector<std::string>& sequences);
    void Clear();
    [[nodiscard]] int Count() const;
    [[nodiscard]] std::string EmojiAt(int index) const;
    [[nodiscard]] std::string TooltipAt(int index) const;
    [[nodiscard]] const std::shared_ptr<const render::DuiImage>& ImageAt(int index) const;
    void SetCellSize(int pixels);
    [[nodiscard]] int CellSize() const;
    void SetColumns(int columns);
    [[nodiscard]] int Columns() const;
    [[nodiscard]] int RowCount() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void SetPickHandler(std::function<void(std::string_view, int)> handler);
    void SetTextStyle(render::DuiTextStyle style);
    [[nodiscard]] core::Rect CellRect(int index) const;
    [[nodiscard]] int HitTestIndex(core::Point point) const;
    [[nodiscard]] int HoveredIndex() const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> emojiPanel_;
};

} // namespace ysDui::controls::feedback
