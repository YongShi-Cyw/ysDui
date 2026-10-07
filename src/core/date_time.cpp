#include "ysDui/core/DuiDateTime.hpp"

namespace ysDui::core {
bool IsLeapYear(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
int DaysInMonth(int year, int month) {
    constexpr int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    return month == 2 && IsLeapYear(year) ? 29 : days[month];
}
bool IsValidDate(int year, int month, int day) { return year >= 1601 && year <= 9999 && day >= 1 && day <= DaysInMonth(year, month); }
bool IsValidTime(int hour, int minute, int second) { return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59; }
int CompareDate(const DateTime& first, const DateTime& second) {
    if (first.year != second.year) return first.year < second.year ? -1 : 1;
    if (first.month != second.month) return first.month < second.month ? -1 : 1;
    if (first.day != second.day) return first.day < second.day ? -1 : 1;
    return 0;
}
int CompareDateTime(const DateTime& first, const DateTime& second) {
    const int date = CompareDate(first, second);
    if (date != 0) return date;
    if (first.hour != second.hour) return first.hour < second.hour ? -1 : 1;
    if (first.minute != second.minute) return first.minute < second.minute ? -1 : 1;
    if (first.second != second.second) return first.second < second.second ? -1 : 1;
    return 0;
}
} // namespace ysDui::core
