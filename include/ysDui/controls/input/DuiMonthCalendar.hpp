#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiDateTime.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiMonthCalendar final : public core::Control, public render::DuiRenderable {
public:
    DuiMonthCalendar();
    ~DuiMonthCalendar() override;
    DuiMonthCalendar(const DuiMonthCalendar&) = delete;
    DuiMonthCalendar& operator=(const DuiMonthCalendar&) = delete;
    DuiMonthCalendar(DuiMonthCalendar&&) = delete;
    DuiMonthCalendar& operator=(DuiMonthCalendar&&) = delete;

    void SetViewMonth(int year, int month);
    [[nodiscard]] int ViewYear() const;
    [[nodiscard]] int ViewMonth() const;
    void SetSelected(core::DateTime value);
    [[nodiscard]] core::DateTime Selected() const;
    void SetMinDate(std::optional<core::DateTime> value);
    void SetMaxDate(std::optional<core::DateTime> value);
    void SetDateSelectedHandler(std::function<void(core::DateTime)> handler);

    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    [[nodiscard]] int DayAtCell(int cell) const;
    [[nodiscard]] core::Point CellFromPoint(core::Point point) const;
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> calendar_;
};

} // namespace ysDui::controls::input
