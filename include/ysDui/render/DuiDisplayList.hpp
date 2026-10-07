#pragma once

#include <memory>

#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

class DisplayList final {
public:
    DisplayList();
    ~DisplayList();
    DisplayList(const DisplayList&) = delete;
    DisplayList& operator=(const DisplayList&) = delete;
    DisplayList(DisplayList&&) noexcept;
    DisplayList& operator=(DisplayList&&) noexcept;

    void FillRect(core::Rect bounds, core::Color color);
    void Clear();
    [[nodiscard]] bool Empty() const;
    void Replay(Canvas& canvas) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::render
