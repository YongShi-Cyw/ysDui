/**
 * 文件名：DuiSpreadsheetInternal.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：保存工作表控件跨实现文件共享的私有状态与辅助类型。
 */
#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/controls/list/DuiMenu.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/render/DuiPath.hpp"

namespace ysDui::controls::list {
namespace spreadsheet_detail {
constexpr int kRowHeaderWidth = 42;
constexpr int kColumnHeaderHeight = 24;
constexpr int kSheetBarHeight = 26;
constexpr int kScrollBarSize = 17;
constexpr int kSheetNavigationButtonWidth = 18;
constexpr int kSheetNavigationButtonCount = 5;
constexpr int kSheetTabWidth = 100;
constexpr int kSheetTabGap = 2;
constexpr int kAddSheetButtonWidth = 28;
constexpr int kDefaultRowHeight = 24;
constexpr int kDefaultColumnWidth = 96;
constexpr int kMinimumDimension = 16;
constexpr int kMaximumDimension = 1000;
constexpr int kViewportPrefetchColumns = 2;
constexpr core::Color kSheetTabHoverColor{207, 207, 207, 255};

enum class ContextCommand : std::uint32_t
{
    Cut = 1,
    Copy,
    Paste,
    ClearContents,
    Find,
    Replace,
    SortAscending,
    SortDescending,
    ClearSort,
    FilterByCell,
    ClearFilters,
    AutoFitColumn,
    AutoFitRow,
    FreezePanes,
    UnfreezePanes,
};

enum class SheetCommand : std::uint32_t
{
    Insert = 1,
    Delete,
    Duplicate,
};

inline int ClampDimension(int value) { return std::clamp(value, kMinimumDimension, kMaximumDimension); }

inline DuiWorkbookWriteResult ReadOnlyWriteResult()
{
    return {DuiWorkbookWriteErrorCode::ReadOnly, "Workbook model marks the cell as read-only"};
}

inline std::string ColumnName(int column)
{
    std::string result;
    for (int value = column + 1; value > 0; value = (value - 1) / 26)
        result.insert(result.begin(), static_cast<char>('A' + (value - 1) % 26));
    return result;
}

class SparseAxis final
{
private:
    struct Override final
    {
        int index{};
        int pixels{};
        int cumulativeDelta{};
    };
    using OverrideList = std::vector<Override>;

public:
    SparseAxis() = default;

    SparseAxis(int count, int defaultPixels, std::vector<DuiAxisSizeOverride> overrides)
        : count_(std::max(0, count)), defaultPixels_(ClampDimension(defaultPixels))
    {
        std::stable_sort(overrides.begin(), overrides.end(), [](const auto& left, const auto& right)
        {
            return left.index < right.index;
        });
        for (const auto& item : overrides)
        {
            if (item.index < 0 || item.index >= count_)
                continue;
            const int pixels = ClampDimension(item.pixels);
            if (!overrides_.empty() && overrides_.back().index == item.index)
                overrides_.back().pixels = pixels;
            else
                overrides_.push_back({item.index, pixels, 0});
        }
        std::erase_if(overrides_, [this](const Override& item)
        {
            return item.pixels == defaultPixels_;
        });
        RebuildCumulative(0);
    }

    [[nodiscard]] int Count() const noexcept { return count_; }
    [[nodiscard]] int DefaultSize() const noexcept { return defaultPixels_; }
    [[nodiscard]] bool Empty() const noexcept { return count_ == 0; }

    [[nodiscard]] int SizeAt(int index) const
    {
        if (index < 0 || index >= count_)
            return defaultPixels_;
        const auto found = LowerBound(index);
        return found != overrides_.end() && found->index == index ? found->pixels : defaultPixels_;
    }

    [[nodiscard]] int OffsetAt(int index) const
    {
        index = std::clamp(index, 0, count_);
        const auto found = LowerBound(index);
        const int cumulative = found == overrides_.begin() ? 0 : std::prev(found)->cumulativeDelta;
        return index * defaultPixels_ + cumulative;
    }

    [[nodiscard]] int TotalSize() const { return OffsetAt(count_); }

    [[nodiscard]] int IndexAt(int position) const
    {
        if (position < 0 || position >= TotalSize())
            return -1;
        int first = 0;
        int last = count_;
        while (first < last)
        {
            const int middle = first + (last - first) / 2;
            if (OffsetAt(middle + 1) <= position)
                first = middle + 1;
            else
                last = middle;
        }
        return first;
    }

    void SetSize(int index, int pixels)
    {
        if (index < 0 || index >= count_)
            return;
        pixels = ClampDimension(pixels);
        auto found = LowerBound(index);
        std::size_t rebuildIndex = static_cast<std::size_t>(std::distance(overrides_.begin(), found));
        if (found != overrides_.end() && found->index == index)
        {
            if (pixels == defaultPixels_)
                overrides_.erase(found);
            else
                found->pixels = pixels;
        }
        else if (pixels != defaultPixels_)
        {
            overrides_.insert(found, {index, pixels, 0});
        }
        RebuildCumulative(rebuildIndex);
    }

private:
    [[nodiscard]] OverrideList::iterator LowerBound(int index)
    {
        return std::lower_bound(overrides_.begin(), overrides_.end(), index,
                                [](const Override& item, int value) { return item.index < value; });
    }
    [[nodiscard]] OverrideList::const_iterator LowerBound(int index) const
    {
        return std::lower_bound(overrides_.begin(), overrides_.end(), index,
                                [](const Override& item, int value) { return item.index < value; });
    }
    void RebuildCumulative(std::size_t first)
    {
        int cumulative = first == 0 ? 0 : overrides_[first - 1].cumulativeDelta;
        for (std::size_t index = first; index < overrides_.size(); ++index)
        {
            cumulative += overrides_[index].pixels - defaultPixels_;
            overrides_[index].cumulativeDelta = cumulative;
        }
    }

    int count_{};
    int defaultPixels_{1};
    OverrideList overrides_;
};

enum class ChangeKind
{
    Cell,
    RowHeight,
    ColumnWidth,
};

enum class SelectionMode
{
    None,
    Cells,
    Rows,
    Columns,
};

enum class FillAxis
{
    None,
    Rows,
    Columns,
};

struct CellChange final
{
    ChangeKind kind{ChangeKind::Cell};
    DuiCellAddress address;
    std::string before;
    std::string after;
    int beforeSize{};
    int afterSize{};
};
} // namespace spreadsheet_detail

using namespace spreadsheet_detail;

class DuiSpreadsheet::Impl
{
public:
    input::DuiScrollBar* horizontal{};
    input::DuiScrollBar* vertical{};
    IDuiWorkbookModel* model{};
    IDuiWorkbookModel* requestedModel{};
    core::DuiSubscription modelSubscription;
    std::shared_ptr<int> modelLifetime;
    std::shared_ptr<int> objectLifetime;
    ui::DuiTextInput* textInput{};
    ui::DuiClipboard* clipboard{};
    ui::IUiHostFactory* popupFactory{};
    ui::HostRef popupOwner;
    render::DuiTextMeasurer* textMeasurer{};
    std::unique_ptr<DuiMenu> contextMenu;
    std::unique_ptr<DuiMenu> sheetMenu;
    std::unique_ptr<DuiMenu> sheetContextMenu;
    std::vector<DuiWorksheetId> sheetMenuSheets;
    std::shared_ptr<int> textInputLifetime;
    std::weak_ptr<void> textInputObjectLifetime;
    DuiWorksheetId sheet{};
    DuiWorksheetId requestedSheet{};
    std::vector<DuiCellRange> requestedRanges;
    DuiCellAddress active{};
    DuiCellAddress anchor{};
    DuiCellRange selection{{}, {}};
    SparseAxis rows;
    SparseAxis columns;
    std::vector<CellChange> editChanges;
    std::vector<std::vector<CellChange>> undo;
    std::vector<std::vector<CellChange>> redo;
    std::function<void(DuiCellAddress)> activeChanged;
    std::function<void(DuiCellRange)> selectionChanged;
    std::function<void(DuiWorksheetId)> sheetChanged;
    std::function<void()> findRequested;
    std::function<void()> replaceRequested;
    std::function<void(const DuiSpreadsheetWriteError&)> workbookWriteError;
    std::function<void(const DuiWorkbookOperationResult&)> workbookOperationError;
    std::vector<DuiWorkbookSortKey> sortKeys;
    std::vector<DuiWorkbookFilter> filters;
    core::Rect viewport{};
    core::Rect columnHeader{};
    core::Rect rowHeader{};
    core::Rect sheetBar{};
    core::Rect firstSheetButton{};
    core::Rect previousSheetButton{};
    core::Rect nextSheetButton{};
    core::Rect lastSheetButton{};
    core::Rect sheetListButton{};
    core::Rect sheetTabArea{};
    core::Rect addSheetButton{};
    core::Rect renameSheetBounds{};
    render::DuiPath selectAllTriangle;
    bool editing{};
    bool renamingSheet{};
    bool selecting{};
    bool fillingSelection{};
    bool movingSelection{};
    SelectionMode selectionMode{SelectionMode::None};
    FillAxis fillAxis{FillAxis::None};
    bool resizingRow{};
    bool resizingColumn{};
    int resizeIndex{-1};
    int resizeStart{};
    int resizeOriginal{};
    DuiCellAddress movePointerCell{};
    DuiCellRange moveSource{{}, {}};
    DuiCellRange moveTarget{{}, {}};
    DuiCellRange fillSource{{}, {}};
    DuiCellRange fillTarget{{}, {}};
    int editRow{-1};
    int editColumn{-1};
    DuiWorksheetId renameSheet{};
    DuiWorksheetId contextSheet{};
    DuiWorksheetId hoveredSheet{};
    int frozenRows{1};
    int frozenColumns{1};
    int firstVisibleSheet{};
    int modelUpdateDepth{};

    class ModelUpdateScope final
    {
    public:
        explicit ModelUpdateScope(Impl& data) : data_(&data), lifetime_(data.objectLifetime)
        {
            ++data_->modelUpdateDepth;
        }
        ~ModelUpdateScope()
        {
            if (lifetime_.lock())
                --data_->modelUpdateDepth;
        }

        ModelUpdateScope(const ModelUpdateScope&) = delete;
        ModelUpdateScope& operator=(const ModelUpdateScope&) = delete;

    private:
        Impl* data_;
        std::weak_ptr<int> lifetime_;
    };

    [[nodiscard]] bool HasSheet() const { return model != nullptr && sheet != 0; }
    [[nodiscard]] bool HasCells() const { return HasSheet() && Rows() > 0 && Columns() > 0; }
    [[nodiscard]] bool HasTextInput() const
    {
        return textInput != nullptr && !textInputObjectLifetime.expired();
    }
    [[nodiscard]] bool HasModelContext(const IDuiWorkbookModel* expectedModel,
                                       DuiWorksheetId expectedSheet) const
    {
        return model == expectedModel && sheet == expectedSheet;
    }
    [[nodiscard]] int Rows() const
    {
        return rows.Count();
    }
    [[nodiscard]] int Columns() const
    {
        return columns.Count();
    }
    [[nodiscard]] int RowSize(int row) const { return rows.SizeAt(row); }
    [[nodiscard]] int ColumnSize(int column) const { return columns.SizeAt(column); }
    [[nodiscard]] int FrozenRowCount() const { return std::clamp(frozenRows, 0, Rows()); }
    [[nodiscard]] int FrozenColumnCount() const { return std::clamp(frozenColumns, 0, Columns()); }
    [[nodiscard]] int FrozenHeight() const { return rows.OffsetAt(FrozenRowCount()); }
    [[nodiscard]] int FrozenWidth() const { return columns.OffsetAt(FrozenColumnCount()); }
    [[nodiscard]] core::Rect CellRect(int row, int column) const
    {
        const int columnOffset = columns.OffsetAt(column);
        const int rowOffset = rows.OffsetAt(row);
        const int left = viewport.left + (column < FrozenColumnCount() ? columnOffset : columnOffset - horizontal->Position());
        const int top = viewport.top + (row < FrozenRowCount() ? rowOffset : rowOffset - vertical->Position());
        return {left, top, left + ColumnSize(column), top + RowSize(row)};
    }
    void RecordUndo(std::vector<CellChange> changes)
    {
        if (changes.empty())
            return;
        undo.push_back(std::move(changes));
        redo.clear();
        if (undo.size() > 100)
            undo.erase(undo.begin());
    }
    [[nodiscard]] bool ApplyCellChanges(const std::vector<CellChange>& changes, bool useBefore);
    void ReportWriteError(DuiWorksheetId worksheet, DuiCellAddress cell, DuiWorkbookWriteResult result,
                          DuiSpreadsheetWriteOperation operation = DuiSpreadsheetWriteOperation::CellText)
    {
        if (workbookWriteError)
            workbookWriteError({worksheet, cell, std::move(result), operation});
    }
    void ReportOperationError(const DuiWorkbookOperationResult& result)
    {
        if (!result && workbookOperationError)
            workbookOperationError(result);
    }
};

} // namespace ysDui::controls::list
