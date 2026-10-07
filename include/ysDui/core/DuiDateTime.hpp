#pragma once

namespace ysDui::core {

struct DateTime final {
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
    int second{};
    constexpr bool operator==(const DateTime&) const = default;
};

[[nodiscard]] bool IsLeapYear(int year);
[[nodiscard]] int DaysInMonth(int year, int month);
[[nodiscard]] bool IsValidDate(int year, int month, int day);
[[nodiscard]] bool IsValidTime(int hour, int minute, int second);
[[nodiscard]] int CompareDate(const DateTime& first, const DateTime& second);
[[nodiscard]] int CompareDateTime(const DateTime& first, const DateTime& second);

} // namespace ysDui::core
