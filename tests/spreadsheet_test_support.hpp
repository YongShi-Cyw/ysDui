/**
 * 文件名：spreadsheet_test_support.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-09
 * 用途：提供工作表行为与生命周期测试共享的模型。
 */
#pragma once

#include "test_support.hpp"
#include "ysDui/controls/list/DuiWorkbookSheetOperations.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

namespace ysDui::test::spreadsheet {

using namespace ysDui;
using namespace ysDui::controls::list;

class WorkbookModel final : public IDuiWorkbookModel, public IDuiWorkbookSheetOperations
{
public:
    struct Sheet final
    {
        DuiWorksheetId id{};
        std::string name;
        std::unordered_map<long long, std::string> cells;
        std::unordered_map<long long, DuiCellPresentation> presentations;
        std::map<int, int> rowHeights;
        std::map<int, int> columnWidths;
    };

    struct ChangeStore final
    {
        std::size_t nextToken{};
        std::vector<std::pair<std::size_t, std::function<void()>>> callbacks;
        std::function<void()> unsubscribeHandler;
    };

    explicit WorkbookModel(bool notifyOnWrite = false) : notifyOnWrite_(notifyOnWrite)
    {
        const DuiWorksheetId first = AddWorksheet("Sheet1");
        const DuiWorksheetId second = AddWorksheet("Sheet2");
        assert(first != 0 && second != 0);
    }

    [[nodiscard]] int WorksheetCount() const override
    {
        if (worksheetCountReadHandler_)
        {
            const auto handler = worksheetCountReadHandler_;
            handler();
        }
        return static_cast<int>(sheets_.size());
    }
    [[nodiscard]] DuiWorksheetId WorksheetIdAt(int index) const override { return index >= 0 && index < WorksheetCount() ? sheets_[index].id : 0; }
    [[nodiscard]] std::string WorksheetName(DuiWorksheetId sheet) const override { return Find(sheet) ? Find(sheet)->name : std::string{}; }
    bool RenameWorksheet(DuiWorksheetId sheet, std::string name) override
    {
        if (renameWorksheetHandler_)
        {
            const auto handler = renameWorksheetHandler_;
            handler();
        }
        if (auto* item = Find(sheet))
        {
            item->name = std::move(name);
            if (notifyOnWrite_) NotifyChanged();
            return true;
        }
        return false;
    }
    [[nodiscard]] DuiWorksheetId AddWorksheet(std::string name) override { const DuiWorksheetId id = nextId_++; sheets_.push_back({id, std::move(name), {}, {}, {}, {}}); if (notifyOnWrite_) NotifyChanged(); return id; }
    bool RemoveWorksheet(DuiWorksheetId sheet) override { auto found = std::find_if(sheets_.begin(), sheets_.end(), [sheet](const Sheet& item) { return item.id == sheet; }); if (found == sheets_.end()) return false; sheets_.erase(found); if (notifyOnWrite_) NotifyChanged(); return true; }
    [[nodiscard]] DuiWorksheetId InsertWorksheetBefore(DuiWorksheetId before, std::string name) override
    {
        const auto found = std::find_if(sheets_.begin(), sheets_.end(),
                                        [before](const Sheet& item) { return item.id == before; });
        if (found == sheets_.end())
            return 0;
        const DuiWorksheetId id = nextId_++;
        sheets_.insert(found, {id, std::move(name), {}, {}, {}, {}});
        if (notifyOnWrite_) NotifyChanged();
        return id;
    }
    [[nodiscard]] DuiWorksheetId DuplicateWorksheet(DuiWorksheetId source, std::string name) override
    {
        const auto found = std::find_if(sheets_.begin(), sheets_.end(),
                                        [source](const Sheet& item) { return item.id == source; });
        if (found == sheets_.end())
            return 0;
        Sheet copy = *found;
        copy.id = nextId_++;
        copy.name = std::move(name);
        const DuiWorksheetId id = copy.id;
        sheets_.insert(std::next(found), std::move(copy));
        if (notifyOnWrite_) NotifyChanged();
        return id;
    }
    [[nodiscard]] int RowCount(DuiWorksheetId) const override
    {
        if (rowCountReadHandler_)
        {
            const auto handler = rowCountReadHandler_;
            handler();
        }
        return rowCount_;
    }
    [[nodiscard]] int ColumnCount(DuiWorksheetId) const override
    {
        if (columnCountReadHandler_)
        {
            const auto handler = columnCountReadHandler_;
            handler();
        }
        return columnCount_;
    }
    void RequestCellRanges(DuiWorksheetId sheet, const std::vector<DuiCellRange>& ranges) override
    {
        lastRequestedSheet_ = sheet;
        requestedRanges_ = ranges;
        ++requestCellRangesCalls_;
        if (requestCellRangesHandler_)
        {
            const auto handler = requestCellRangesHandler_;
            handler();
        }
    }
    [[nodiscard]] std::string CellText(DuiWorksheetId sheet, DuiCellAddress cell) const override
    {
        if (cellReadHandler_)
        {
            const auto handler = cellReadHandler_;
            handler();
        }
        if (const auto* item = Find(sheet))
        {
            const auto found = item->cells.find(Key(cell));
            return found == item->cells.end() ? std::string{} : found->second;
        }
        return {};
    }
    [[nodiscard]] DuiWorkbookCapability Capabilities() const noexcept override
    {
        return capabilities_;
    }
    [[nodiscard]] DuiCellPresentation CellPresentation(DuiWorksheetId sheet, DuiCellAddress cell) const override
    {
        if (cellPresentationReadHandler_)
        {
            const auto handler = cellPresentationReadHandler_;
            handler();
        }
        if (const auto* item = Find(sheet))
        {
            const auto found = item->presentations.find(Key(cell));
            if (found != item->presentations.end())
                return found->second;
        }
        return IDuiWorkbookModel::CellPresentation(sheet, cell);
    }
    [[nodiscard]] bool IsCellReadOnly(DuiWorksheetId, DuiCellAddress cell) const override
    {
        return cellReadOnly_ && cell.Valid();
    }
    bool SetCellText(DuiWorksheetId sheet, DuiCellAddress cell, std::string text) override { auto* item = Find(sheet); if (!cellWritesAllowed_ || !item || !cell.Valid()) return false; if (text.empty()) item->cells.erase(Key(cell)); else item->cells[Key(cell)] = std::move(text); if (notifyOnWrite_) NotifyChanged(); return true; }
    [[nodiscard]] DuiWorkbookWriteResult SetCellTextWithResult(
        DuiWorksheetId sheet, DuiCellAddress cell, std::string text) override
    {
        if (cellWriteFailure_)
            return *cellWriteFailure_;
        if (cellWriteFailureAfter_ && cellWriteCalls_++ == *cellWriteFailureAfter_)
            return {DuiWorkbookWriteErrorCode::Rejected, "Injected cell write failure"};
        return IDuiWorkbookModel::SetCellTextWithResult(sheet, cell, std::move(text));
    }
    [[nodiscard]] int RowHeight(DuiWorksheetId sheet, int row) const override
    {
        ++rowHeightReads_;
        if (rowHeightReadHandler_)
        {
            const auto handler = rowHeightReadHandler_;
            handler();
        }
        if (const auto* item = Find(sheet))
        {
            const auto found = item->rowHeights.find(row);
            if (found != item->rowHeights.end())
                return found->second;
        }
        return 24;
    }
    [[nodiscard]] int ColumnWidth(DuiWorksheetId sheet, int column) const override
    {
        ++columnWidthReads_;
        if (columnWidthReadHandler_)
        {
            const auto handler = columnWidthReadHandler_;
            handler();
        }
        if (const auto* item = Find(sheet))
        {
            const auto found = item->columnWidths.find(column);
            if (found != item->columnWidths.end())
                return found->second;
        }
        return 96;
    }
    [[nodiscard]] std::optional<DuiSparseAxisSizes> SparseRowSizes(DuiWorksheetId) const override
    {
        ++sparseRowSizeReads_;
        if (sparseRowSizeReadHandler_)
        {
            const auto handler = sparseRowSizeReadHandler_;
            handler();
        }
        return sparseRowSizes_;
    }
    [[nodiscard]] std::optional<DuiSparseAxisSizes> SparseColumnSizes(DuiWorksheetId) const override
    {
        ++sparseColumnSizeReads_;
        if (sparseColumnSizeReadHandler_)
        {
            const auto handler = sparseColumnSizeReadHandler_;
            handler();
        }
        return sparseColumnSizes_;
    }
    bool SetRowHeight(DuiWorksheetId sheet, int row, int pixels) override { if (!dimensionWritesAllowed_) return false; if (auto* item = Find(sheet)) { item->rowHeights[row] = pixels; if (notifyOnWrite_) NotifyChanged(); return true; } return false; }
    bool SetColumnWidth(DuiWorksheetId sheet, int column, int pixels) override { if (!dimensionWritesAllowed_) return false; if (auto* item = Find(sheet)) { item->columnWidths[column] = pixels; if (notifyOnWrite_) NotifyChanged(); return true; } return false; }
    [[nodiscard]] DuiWorkbookWriteResult SetRowHeightWithResult(
        DuiWorksheetId sheet, int row, int pixels) override
    {
        if (dimensionWriteFailure_)
            return *dimensionWriteFailure_;
        return IDuiWorkbookModel::SetRowHeightWithResult(sheet, row, pixels);
    }
    [[nodiscard]] DuiWorkbookWriteResult SetColumnWidthWithResult(
        DuiWorksheetId sheet, int column, int pixels) override
    {
        if (dimensionWriteFailure_)
            return *dimensionWriteFailure_;
        return IDuiWorkbookModel::SetColumnWidthWithResult(sheet, column, pixels);
    }
    [[nodiscard]] DuiWorkbookFindResult FindCell(
        DuiWorksheetId sheet, std::string_view query, std::optional<DuiCellAddress> startAfter,
        std::optional<DuiCellRange> range, DuiWorkbookFindOptions options) const override
    {
        lastFindSheet_ = sheet;
        lastFindQuery_ = query;
        lastFindStart_ = startAfter;
        lastFindRange_ = range;
        lastFindOptions_ = options;
        ++findCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return findResult_;
    }
    [[nodiscard]] DuiWorkbookReplaceResult ReplaceText(
        DuiWorksheetId sheet, std::string_view query, std::string_view replacement,
        std::optional<DuiCellRange> range, DuiWorkbookFindOptions options, bool replaceAll) override
    {
        lastReplaceSheet_ = sheet;
        lastReplaceQuery_ = query;
        lastReplacement_ = replacement;
        lastReplaceRange_ = range;
        lastReplaceOptions_ = options;
        lastReplaceAll_ = replaceAll;
        ++replaceCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return replaceResult_;
    }
    [[nodiscard]] DuiWorkbookOperationResult ApplySort(
        DuiWorksheetId sheet, DuiCellRange range, const std::vector<DuiWorkbookSortKey>& keys) override
    {
        lastSortSheet_ = sheet;
        lastSortRange_ = range;
        lastSortKeys_ = keys;
        ++sortCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return operationResult_;
    }
    [[nodiscard]] DuiWorkbookOperationResult SetFilters(
        DuiWorksheetId sheet, const std::vector<DuiWorkbookFilter>& filters) override
    {
        lastFilterSheet_ = sheet;
        lastFilters_ = filters;
        ++filterCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return operationResult_;
    }
    [[nodiscard]] std::optional<int> PreferredRowHeight(DuiWorksheetId, int) const override
    {
        ++preferredRowHeightCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return preferredRowHeight_;
    }
    [[nodiscard]] std::optional<int> PreferredColumnWidth(DuiWorksheetId, int) const override
    {
        ++preferredColumnWidthCalls_;
        if (operationHandler_)
        {
            const auto handler = operationHandler_;
            handler();
        }
        return preferredColumnWidth_;
    }
    [[nodiscard]] core::DuiSubscription SubscribeChanged(std::function<void()> callback) override
    {
        if (!callback)
            return {};
        const auto store = changeStore_;
        const std::size_t token = store->nextToken++;
        store->callbacks.emplace_back(token, std::move(callback));
        if (subscribeHandler_)
        {
            const auto handler = subscribeHandler_;
            handler();
        }
        return core::DuiSubscription::FromUnsubscribe([weakStore = std::weak_ptr<ChangeStore>{store}, token]
        {
            if (const auto locked = weakStore.lock())
            {
                std::erase_if(locked->callbacks, [token](const auto& item) { return item.first == token; });
                if (locked->unsubscribeHandler)
                {
                    const auto handler = locked->unsubscribeHandler;
                    handler();
                }
            }
        });
    }
    void NotifyChanged()
    {
        const auto callbacks = changeStore_->callbacks;
        for (const auto& item : callbacks)
            if (item.second)
                item.second();
    }
    [[nodiscard]] std::size_t SubscriptionCount() const { return changeStore_->callbacks.size(); }
    bool SetPresentation(DuiWorksheetId sheet, DuiCellAddress cell, DuiCellPresentation presentation) { if (auto* item = Find(sheet)) { item->presentations[Key(cell)] = std::move(presentation); return true; } return false; }
    void SetCapabilities(DuiWorkbookCapability capabilities) { capabilities_ = capabilities; }
    void SetFindResult(DuiWorkbookFindResult result) { findResult_ = std::move(result); }
    void SetReplaceResult(DuiWorkbookReplaceResult result) { replaceResult_ = std::move(result); }
    void SetOperationResult(DuiWorkbookOperationResult result) { operationResult_ = std::move(result); }
    void SetPreferredRowHeight(std::optional<int> pixels) { preferredRowHeight_ = pixels; }
    void SetPreferredColumnWidth(std::optional<int> pixels) { preferredColumnWidth_ = pixels; }
    void SetOperationHandler(std::function<void()> handler) { operationHandler_ = std::move(handler); }
    [[nodiscard]] std::size_t FindCalls() const { return findCalls_; }
    [[nodiscard]] std::size_t ReplaceCalls() const { return replaceCalls_; }
    [[nodiscard]] std::size_t SortCalls() const { return sortCalls_; }
    [[nodiscard]] std::size_t FilterCalls() const { return filterCalls_; }
    [[nodiscard]] std::size_t PreferredRowHeightCalls() const { return preferredRowHeightCalls_; }
    [[nodiscard]] std::size_t PreferredColumnWidthCalls() const { return preferredColumnWidthCalls_; }
    [[nodiscard]] const std::string& LastFindQuery() const { return lastFindQuery_; }
    [[nodiscard]] std::optional<DuiCellRange> LastFindRange() const { return lastFindRange_; }
    [[nodiscard]] const std::vector<DuiWorkbookSortKey>& LastSortKeys() const { return lastSortKeys_; }
    [[nodiscard]] const std::vector<DuiWorkbookFilter>& LastFilters() const { return lastFilters_; }
    void SetCellWritesAllowed(bool allowed) { cellWritesAllowed_ = allowed; }
    void SetCellReadOnly(bool readOnly) { cellReadOnly_ = readOnly; }
    void SetCellWriteFailure(std::optional<DuiWorkbookWriteResult> failure)
    {
        cellWriteFailure_ = std::move(failure);
    }
    void SetCellWriteFailureAfter(std::optional<std::size_t> successfulWrites)
    {
        cellWriteFailureAfter_ = successfulWrites;
        cellWriteCalls_ = 0;
    }
    void SetDimensionWriteFailure(std::optional<DuiWorkbookWriteResult> failure)
    {
        dimensionWriteFailure_ = std::move(failure);
    }
    void SetDimensionWritesAllowed(bool allowed) { dimensionWritesAllowed_ = allowed; }
    void SetDimensions(int rows, int columns)
    {
        rowCount_ = rows;
        columnCount_ = columns;
    }
    void SetSparseRowSizes(std::optional<DuiSparseAxisSizes> sizes) { sparseRowSizes_ = std::move(sizes); }
    void SetSparseColumnSizes(std::optional<DuiSparseAxisSizes> sizes) { sparseColumnSizes_ = std::move(sizes); }
    void ResetDimensionReadCounts() const
    {
        rowHeightReads_ = 0;
        columnWidthReads_ = 0;
        sparseRowSizeReads_ = 0;
        sparseColumnSizeReads_ = 0;
    }
    [[nodiscard]] std::size_t RowHeightReads() const { return rowHeightReads_; }
    [[nodiscard]] std::size_t ColumnWidthReads() const { return columnWidthReads_; }
    [[nodiscard]] std::size_t SparseRowSizeReads() const { return sparseRowSizeReads_; }
    [[nodiscard]] std::size_t SparseColumnSizeReads() const { return sparseColumnSizeReads_; }
    [[nodiscard]] std::size_t RequestCellRangesCalls() const { return requestCellRangesCalls_; }
    [[nodiscard]] DuiWorksheetId LastRequestedSheet() const { return lastRequestedSheet_; }
    [[nodiscard]] const std::vector<DuiCellRange>& RequestedRanges() const { return requestedRanges_; }
    void ResetCellRangeRequests()
    {
        requestCellRangesCalls_ = 0;
        lastRequestedSheet_ = 0;
        requestedRanges_.clear();
    }
    void SetCellReadHandler(std::function<void()> handler) { cellReadHandler_ = std::move(handler); }
    void SetCellPresentationReadHandler(std::function<void()> handler)
    {
        cellPresentationReadHandler_ = std::move(handler);
    }
    void SetRowCountReadHandler(std::function<void()> handler) { rowCountReadHandler_ = std::move(handler); }
    void SetColumnCountReadHandler(std::function<void()> handler) { columnCountReadHandler_ = std::move(handler); }
    void SetRowHeightReadHandler(std::function<void()> handler) { rowHeightReadHandler_ = std::move(handler); }
    void SetColumnWidthReadHandler(std::function<void()> handler)
    {
        columnWidthReadHandler_ = std::move(handler);
    }
    void SetSparseRowSizeReadHandler(std::function<void()> handler)
    {
        sparseRowSizeReadHandler_ = std::move(handler);
    }
    void SetSparseColumnSizeReadHandler(std::function<void()> handler)
    {
        sparseColumnSizeReadHandler_ = std::move(handler);
    }
    void SetRequestCellRangesHandler(std::function<void()> handler)
    {
        requestCellRangesHandler_ = std::move(handler);
    }
    void SetWorksheetCountReadHandler(std::function<void()> handler) { worksheetCountReadHandler_ = std::move(handler); }
    void SetRenameWorksheetHandler(std::function<void()> handler) { renameWorksheetHandler_ = std::move(handler); }
    void SetSubscribeHandler(std::function<void()> handler) { subscribeHandler_ = std::move(handler); }
    void SetUnsubscribeHandler(std::function<void()> handler)
    {
        changeStore_->unsubscribeHandler = std::move(handler);
    }

private:
    static long long Key(DuiCellAddress cell) { return (static_cast<long long>(cell.row) << 32) | static_cast<unsigned int>(cell.column); }
    Sheet* Find(DuiWorksheetId sheet) { auto found = std::find_if(sheets_.begin(), sheets_.end(), [sheet](const Sheet& item) { return item.id == sheet; }); return found == sheets_.end() ? nullptr : &*found; }
    const Sheet* Find(DuiWorksheetId sheet) const { return const_cast<WorkbookModel*>(this)->Find(sheet); }
    std::vector<Sheet> sheets_;
    DuiWorksheetId nextId_{1};
    std::shared_ptr<ChangeStore> changeStore_{std::make_shared<ChangeStore>()};
    bool notifyOnWrite_{};
    bool cellWritesAllowed_{true};
    bool cellReadOnly_{};
    std::optional<DuiWorkbookWriteResult> cellWriteFailure_;
    std::optional<std::size_t> cellWriteFailureAfter_;
    std::size_t cellWriteCalls_{};
    std::optional<DuiWorkbookWriteResult> dimensionWriteFailure_;
    std::optional<DuiSparseAxisSizes> sparseRowSizes_;
    std::optional<DuiSparseAxisSizes> sparseColumnSizes_;
    bool dimensionWritesAllowed_{true};
    DuiWorkbookCapability capabilities_{DuiWorkbookCapability::None};
    DuiWorkbookFindResult findResult_;
    DuiWorkbookReplaceResult replaceResult_;
    DuiWorkbookOperationResult operationResult_;
    std::optional<int> preferredRowHeight_;
    std::optional<int> preferredColumnWidth_;
    mutable DuiWorksheetId lastFindSheet_{};
    mutable std::string lastFindQuery_;
    mutable std::optional<DuiCellAddress> lastFindStart_;
    mutable std::optional<DuiCellRange> lastFindRange_;
    mutable DuiWorkbookFindOptions lastFindOptions_;
    DuiWorksheetId lastReplaceSheet_{};
    std::string lastReplaceQuery_;
    std::string lastReplacement_;
    std::optional<DuiCellRange> lastReplaceRange_;
    DuiWorkbookFindOptions lastReplaceOptions_;
    bool lastReplaceAll_{};
    DuiWorksheetId lastSortSheet_{};
    DuiCellRange lastSortRange_;
    std::vector<DuiWorkbookSortKey> lastSortKeys_;
    DuiWorksheetId lastFilterSheet_{};
    std::vector<DuiWorkbookFilter> lastFilters_;
    mutable std::size_t findCalls_{};
    std::size_t replaceCalls_{};
    std::size_t sortCalls_{};
    std::size_t filterCalls_{};
    mutable std::size_t preferredRowHeightCalls_{};
    mutable std::size_t preferredColumnWidthCalls_{};
    mutable std::function<void()> operationHandler_;
    int rowCount_{100000};
    int columnCount_{1000};
    mutable std::function<void()> cellReadHandler_;
    mutable std::function<void()> cellPresentationReadHandler_;
    mutable std::function<void()> rowCountReadHandler_;
    mutable std::function<void()> columnCountReadHandler_;
    mutable std::function<void()> rowHeightReadHandler_;
    mutable std::function<void()> columnWidthReadHandler_;
    mutable std::function<void()> sparseRowSizeReadHandler_;
    mutable std::function<void()> sparseColumnSizeReadHandler_;
    mutable std::function<void()> worksheetCountReadHandler_;
    std::function<void()> renameWorksheetHandler_;
    mutable std::function<void()> subscribeHandler_;
    mutable std::size_t rowHeightReads_{};
    mutable std::size_t columnWidthReads_{};
    mutable std::size_t sparseRowSizeReads_{};
    mutable std::size_t sparseColumnSizeReads_{};
    std::size_t requestCellRangesCalls_{};
    DuiWorksheetId lastRequestedSheet_{};
    std::vector<DuiCellRange> requestedRanges_;
    std::function<void()> requestCellRangesHandler_;
};

} // namespace ysDui::test::spreadsheet
