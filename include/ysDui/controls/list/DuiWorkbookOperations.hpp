/**
 * 文件名：DuiWorkbookOperations.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：声明工作表查找、替换、排序和筛选的结构化操作契约。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ysDui::controls::list {

struct DuiCellAddress final
{
    int row{};
    int column{};

    [[nodiscard]] constexpr bool Valid() const noexcept { return row >= 0 && column >= 0; }
    [[nodiscard]] constexpr bool operator==(const DuiCellAddress&) const noexcept = default;
};

struct DuiCellRange final
{
    DuiCellAddress first;
    DuiCellAddress last;

    [[nodiscard]] constexpr DuiCellRange Normalized() const noexcept
    {
        return {{first.row < last.row ? first.row : last.row,
                 first.column < last.column ? first.column : last.column},
                {first.row < last.row ? last.row : first.row,
                 first.column < last.column ? last.column : first.column}};
    }
    [[nodiscard]] constexpr bool Contains(DuiCellAddress cell) const noexcept
    {
        const DuiCellRange range = Normalized();
        return cell.row >= range.first.row && cell.row <= range.last.row
            && cell.column >= range.first.column && cell.column <= range.last.column;
    }
    [[nodiscard]] constexpr bool operator==(const DuiCellRange&) const noexcept = default;
};

enum class DuiWorkbookOperationErrorCode
{
    None,
    Unsupported,
    InvalidWorksheet,
    InvalidRange,
    InvalidArgument,
    Rejected,
};

/** \brief 工作簿数据操作结果，message 使用 UTF-8。 */
struct DuiWorkbookOperationResult final
{
    DuiWorkbookOperationErrorCode code{DuiWorkbookOperationErrorCode::None};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return code == DuiWorkbookOperationErrorCode::None;
    }
};

enum class DuiWorkbookCapability : std::uint32_t
{
    None = 0,
    Find = 1U << 0U,
    Replace = 1U << 1U,
    Sort = 1U << 2U,
    Filter = 1U << 3U,
};

[[nodiscard]] constexpr DuiWorkbookCapability operator|(DuiWorkbookCapability left,
                                                         DuiWorkbookCapability right) noexcept
{
    return static_cast<DuiWorkbookCapability>(static_cast<std::uint32_t>(left)
                                              | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool HasCapability(DuiWorkbookCapability capabilities,
                                           DuiWorkbookCapability requested) noexcept
{
    return (static_cast<std::uint32_t>(capabilities) & static_cast<std::uint32_t>(requested))
        == static_cast<std::uint32_t>(requested);
}

struct DuiWorkbookFindOptions final
{
    bool matchCase{};
    bool matchWholeCell{};
    bool backward{};
    bool wrap{true};
};

struct DuiWorkbookFindResult final
{
    DuiWorkbookOperationResult operation;
    std::optional<DuiCellAddress> cell;
};

struct DuiWorkbookReplaceResult final
{
    DuiWorkbookOperationResult operation;
    std::size_t replacedCount{};
    std::optional<DuiCellAddress> firstChanged;
};

enum class DuiWorkbookSortDirection
{
    Ascending,
    Descending,
};

struct DuiWorkbookSortKey final
{
    int column{};
    DuiWorkbookSortDirection direction{DuiWorkbookSortDirection::Ascending};

    [[nodiscard]] constexpr bool operator==(const DuiWorkbookSortKey&) const noexcept = default;
};

enum class DuiWorkbookFilterOperator
{
    Equals,
    NotEquals,
    Contains,
    BeginsWith,
    EndsWith,
    IsEmpty,
    IsNotEmpty,
};

struct DuiWorkbookFilter final
{
    int column{};
    DuiWorkbookFilterOperator operation{DuiWorkbookFilterOperator::Equals};
    std::string value;
    bool matchCase{};

    [[nodiscard]] bool operator==(const DuiWorkbookFilter&) const noexcept = default;
};

[[nodiscard]] inline DuiWorkbookOperationResult UnsupportedWorkbookOperation()
{
    return {DuiWorkbookOperationErrorCode::Unsupported, "Workbook model does not support this operation"};
}

} // namespace ysDui::controls::list
