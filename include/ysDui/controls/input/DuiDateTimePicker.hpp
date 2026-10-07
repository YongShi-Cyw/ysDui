#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/core/DuiDateTime.hpp"
#include "ysDui/ui/DuiDateTimeSource.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::input {

class DuiDateTimePicker final : public core::Control, public render::DuiRenderable {
public:
    enum class Mode { Date, Time, DateTime };

    DuiDateTimePicker();
    ~DuiDateTimePicker() override;
    DuiDateTimePicker(const DuiDateTimePicker&) = delete;
    DuiDateTimePicker& operator=(const DuiDateTimePicker&) = delete;
    DuiDateTimePicker(DuiDateTimePicker&&) = delete;
    DuiDateTimePicker& operator=(DuiDateTimePicker&&) = delete;

    void SetMode(Mode mode);
    [[nodiscard]] Mode GetMode() const;
    void SetDate(core::DateTime value, bool notify = false);
    [[nodiscard]] core::DateTime Date() const;
    void SetFormat(std::string format);
    [[nodiscard]] const std::string& Format() const;
    void SetMinDate(std::optional<core::DateTime> value);
    void SetMaxDate(std::optional<core::DateTime> value);
    void SetDateTimeSource(const ui::DuiDateTimeSource* source);
    void SetPopupHost(ui::IPopupHost* popup);
    void SetValueChangedHandler(std::function<void(core::DateTime)> handler);
    [[nodiscard]] bool PopupOpen() const;
    void OpenPopup();
    void ClosePopup();

    [[nodiscard]] static bool TryParseYmd(std::string_view text, core::DateTime& value);
    [[nodiscard]] static bool TryParseHms(std::string_view text, core::DateTime& value);
    [[nodiscard]] static bool TryParseYmdHms(std::string_view text, core::DateTime& value);
    [[nodiscard]] static std::string FormatDate(core::DateTime value, std::string_view format);

    [[nodiscard]] core::Rect DropButtonRect() const;
    [[nodiscard]] core::Size DesiredSize() const override;
    void Layout(core::Rect bounds);
    bool OnEvent(const core::Event& event) override;
    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

private:
    class Impl;
    std::unique_ptr<Impl> picker_;
};

} // namespace ysDui::controls::input
