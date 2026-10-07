/**
 * 文件名：DuiWin32Utf8.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：封装 Win32 后端私有的 UTF-8 与 UTF-16 转换。
 */
#pragma once

#include <windows.h>

#include <limits>
#include <string>
#include <string_view>

namespace ysDui::platform::win32::detail {

inline std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty() || value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return {};

    const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                                static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0)
        return {};

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                              result.data(), required) != required)
        return {};
    return result;
}

inline std::string WideToUtf8(std::wstring_view value)
{
    if (value.empty() || value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return {};

    const int required = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                                static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0)
        return {};

    std::string result(static_cast<std::size_t>(required), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                              result.data(), required, nullptr, nullptr) != required)
        return {};
    return result;
}

} // namespace ysDui::platform::win32::detail
