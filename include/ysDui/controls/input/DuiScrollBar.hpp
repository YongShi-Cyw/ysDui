#pragma once

#include <functional>
#include <memory>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiScrollBar final : public core::Control, public render::DuiRenderable {
public:
    static constexpr int MinimumThumbPixels = 18;

    explicit DuiScrollBar(bool horizontal = false);
    ~DuiScrollBar() override;
    DuiScrollBar(const DuiScrollBar&) = delete;
    DuiScrollBar& operator=(const DuiScrollBar&) = delete;
    DuiScrollBar(DuiScrollBar&&) noexcept;
    DuiScrollBar& operator=(DuiScrollBar&&) noexcept;

    void SetHorizontal(bool horizontal);
    [[nodiscard]] bool Horizontal() const;
    void SetRange(int minimum, int maximum);
    [[nodiscard]] int Minimum() const;
    [[nodiscard]] int Maximum() const;
    void SetPageSize(int pageSize);
    [[nodiscard]] int PageSize() const;
    void SetPosition(int position, bool notify = true);
    [[nodiscard]] int Position() const;
    void SetLineSize(int lineSize);
    [[nodiscard]] int LineSize() const;
    void SetValueChangedHandler(std::function<void(int)> handler);
    void SetTrackColor(core::Color color);
    void SetThumbColor(core::Color color);

    [[nodiscard]] core::Rect ComputeThumbRect() const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> scrollBar_;
};

} // namespace ysDui::controls::input
