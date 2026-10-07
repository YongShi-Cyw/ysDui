/**
 * 文件名：performance_baseline.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-31
 * 用途：采集布局、虚拟列表、控件树和 Win32 文本渲染的可重复性能基线。
 */
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

#if defined(YSDUI_PERFORMANCE_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

#include "ysDui/controls/layout/DuiStack.hpp"
#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/controls/list/DuiVirtualList.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiCanvas.hpp"
#include "ysDui/ui/DuiClipboard.hpp"

#if defined(YSDUI_PERFORMANCE_WIN32)
#include "ysDui/platform/win32/DuiWin32OffscreenRenderer.hpp"
#endif

namespace {

thread_local std::size_t* allocationCounter{};

[[nodiscard]] void* Allocate(std::size_t size, std::size_t alignment)
{
    size = (std::max)(size, std::size_t{1});
    if (alignment <= alignof(std::max_align_t))
    {
        if (void* result = std::malloc(size))
            return result;
    }
    else
    {
#if defined(_MSC_VER) || defined(__MINGW32__)
        if (void* result = _aligned_malloc(size, alignment))
            return result;
#else
        const std::size_t alignedSize = ((size + alignment - 1) / alignment) * alignment;
        if (void* result = std::aligned_alloc(alignment, alignedSize))
            return result;
#endif
    }
    throw std::bad_alloc();
}

void DeallocateAligned(void* value) noexcept
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(value);
#else
    std::free(value);
#endif
}

} // namespace

void* operator new(std::size_t size)
{
    if (allocationCounter != nullptr)
        ++*allocationCounter;
    return Allocate(size, alignof(std::max_align_t));
}

void* operator new[](std::size_t size)
{
    if (allocationCounter != nullptr)
        ++*allocationCounter;
    return Allocate(size, alignof(std::max_align_t));
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (allocationCounter != nullptr)
        ++*allocationCounter;
    return Allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    if (allocationCounter != nullptr)
        ++*allocationCounter;
    return Allocate(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }
void operator delete(void* value, std::align_val_t) noexcept { DeallocateAligned(value); }
void operator delete[](void* value, std::align_val_t) noexcept { DeallocateAligned(value); }
void operator delete(void* value, std::size_t, std::align_val_t) noexcept { DeallocateAligned(value); }
void operator delete[](void* value, std::size_t, std::align_val_t) noexcept { DeallocateAligned(value); }

namespace {

using Clock = std::chrono::steady_clock;

class AllocationScope final
{
public:
    explicit AllocationScope(std::size_t& count) : previous_(allocationCounter)
    {
        count = 0;
        allocationCounter = &count;
    }

    ~AllocationScope() { allocationCounter = previous_; }

    AllocationScope(const AllocationScope&) = delete;
    AllocationScope& operator=(const AllocationScope&) = delete;

private:
    std::size_t* previous_{};
};

class BenchmarkCanvas final : public ysDui::render::Canvas
{
public:
    void PushClip(ysDui::core::Rect) override {}
    void PopClip() override {}
    void FillRect(ysDui::core::Rect, ysDui::core::Color) override { ++drawCalls; }
    void FillRoundedRect(ysDui::core::Rect, int, ysDui::core::Color) override { ++drawCalls; }
    void FillLinearGradient(ysDui::core::Rect, int, const ysDui::render::DuiLinearGradient&) override
    {
        ++drawCalls;
    }
    void FillRadialGradient(ysDui::core::Rect, int, const ysDui::render::DuiRadialGradient&) override
    {
        ++drawCalls;
    }
    void StrokeRoundedRect(ysDui::core::Rect, int, ysDui::core::Color, float) override { ++drawCalls; }
    void FillEllipse(ysDui::core::Rect, ysDui::core::Color) override { ++drawCalls; }
    void StrokeArc(ysDui::core::Rect, float, float, ysDui::core::Color, float) override { ++drawCalls; }
    void FillPath(const ysDui::render::DuiPath&, ysDui::core::Color) override { ++drawCalls; }
    void StrokePath(const ysDui::render::DuiPath&, ysDui::core::Color, float) override { ++drawCalls; }
    void StrokeCubicBezier(ysDui::core::Point, ysDui::core::Point, ysDui::core::Point, ysDui::core::Point,
                           ysDui::core::Color, float) override
    {
        ++drawCalls;
    }
    void DrawText(std::string_view, ysDui::core::Rect, const ysDui::render::DuiTextStyle&,
                  ysDui::render::DuiTextAlignment, bool) override
    {
        ++drawCalls;
    }
    ysDui::render::DuiTextMetrics MeasureText(
        std::string_view text, const ysDui::render::DuiTextStyle&,
        const ysDui::render::DuiTextMeasureOptions&) override
    {
        return {{static_cast<int>(text.size()) * 8, 16}, 1, 12};
    }
    void DrawImage(const ysDui::render::DuiImage&, ysDui::core::Rect) override { ++drawCalls; }
    void DrawImage(const ysDui::render::DuiImage&, ysDui::core::Rect, ysDui::core::Rect) override
    {
        ++drawCalls;
    }
    void DrawImageEllipse(const ysDui::render::DuiImage&, ysDui::core::Rect) override { ++drawCalls; }
    void DrawImageRounded(const ysDui::render::DuiImage&, ysDui::core::Rect, int) override { ++drawCalls; }

    std::size_t drawCalls{};
};

class SpreadsheetBenchmarkModel final : public ysDui::controls::list::IDuiWorkbookModel
{
public:
    [[nodiscard]] int WorksheetCount() const override { return 1; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId WorksheetIdAt(int) const override { return 1; }
    [[nodiscard]] std::string WorksheetName(ysDui::controls::list::DuiWorksheetId) const override { return "Benchmark"; }
    bool RenameWorksheet(ysDui::controls::list::DuiWorksheetId, std::string) override { return true; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId AddWorksheet(std::string) override { return 0; }
    bool RemoveWorksheet(ysDui::controls::list::DuiWorksheetId) override { return false; }
    [[nodiscard]] int RowCount(ysDui::controls::list::DuiWorksheetId) const override { return 100000; }
    [[nodiscard]] int ColumnCount(ysDui::controls::list::DuiWorksheetId) const override { return 1000; }
    [[nodiscard]] std::string CellText(ysDui::controls::list::DuiWorksheetId, ysDui::controls::list::DuiCellAddress cell) const override { ++cellTextReads; return cell.row == 0 && cell.column == 0 ? "Value" : std::string{}; }
    bool SetCellText(ysDui::controls::list::DuiWorksheetId,
                     ysDui::controls::list::DuiCellAddress, std::string) override
    {
        ++cellTextWrites;
        return true;
    }
    [[nodiscard]] int RowHeight(ysDui::controls::list::DuiWorksheetId, int row) const override
    {
        ++rowHeightReads;
        const auto found = rowHeights.find(row);
        return found == rowHeights.end() ? 24 : found->second;
    }
    [[nodiscard]] int ColumnWidth(ysDui::controls::list::DuiWorksheetId, int column) const override
    {
        ++columnWidthReads;
        const auto found = columnWidths.find(column);
        return found == columnWidths.end() ? 96 : found->second;
    }
    [[nodiscard]] std::optional<ysDui::controls::list::DuiSparseAxisSizes> SparseRowSizes(
        ysDui::controls::list::DuiWorksheetId) const override
    {
        ++sparseRowSizeReads;
        ysDui::controls::list::DuiSparseAxisSizes result{24, {}};
        result.overrides.reserve(rowHeights.size());
        for (const auto& [index, pixels] : rowHeights)
            result.overrides.push_back({index, pixels});
        return result;
    }
    [[nodiscard]] std::optional<ysDui::controls::list::DuiSparseAxisSizes> SparseColumnSizes(
        ysDui::controls::list::DuiWorksheetId) const override
    {
        ++sparseColumnSizeReads;
        ysDui::controls::list::DuiSparseAxisSizes result{96, {}};
        result.overrides.reserve(columnWidths.size());
        for (const auto& [index, pixels] : columnWidths)
            result.overrides.push_back({index, pixels});
        return result;
    }
    bool SetRowHeight(ysDui::controls::list::DuiWorksheetId, int row, int pixels) override
    {
        rowHeights[row] = pixels;
        return true;
    }
    bool SetColumnWidth(ysDui::controls::list::DuiWorksheetId, int column, int pixels) override
    {
        columnWidths[column] = pixels;
        return true;
    }

    void ResetAccessCounts() const
    {
        cellTextReads = rowHeightReads = columnWidthReads = 0;
        sparseRowSizeReads = sparseColumnSizeReads = 0;
        cellTextWrites = 0;
    }

    mutable std::size_t cellTextReads{};
    mutable std::size_t cellTextWrites{};
    mutable std::size_t rowHeightReads{};
    mutable std::size_t columnWidthReads{};
    mutable std::size_t sparseRowSizeReads{};
    mutable std::size_t sparseColumnSizeReads{};
    std::unordered_map<int, int> rowHeights;
    std::unordered_map<int, int> columnWidths;
};

class BenchmarkClipboard final : public ysDui::ui::DuiClipboard
{
public:
    bool SetText(std::string value) override
    {
        text = std::move(value);
        return true;
    }
    [[nodiscard]] std::optional<std::string> GetText() const override { return text; }

    std::string text;
};

template<typename Action>
long long MeasureMedianMicroseconds(Action action)
{
    std::vector<long long> samples;
    samples.reserve(3);
    for (int sample = 0; sample < 3; ++sample)
    {
        const auto start = Clock::now();
        action();
        samples.push_back(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[1];
}

void ResetSpreadsheetScroll(ysDui::controls::list::DuiSpreadsheet& spreadsheet)
{
    for (const auto& child : spreadsheet.Children())
    {
        auto* scrollBar = dynamic_cast<ysDui::controls::input::DuiScrollBar*>(child.get());
        if (scrollBar != nullptr)
            scrollBar->SetPosition(0);
    }
}

std::string MakeTsv(int rows, int columns)
{
    std::string result;
    for (int row = 0; row < rows; ++row)
    {
        if (row != 0)
            result += "\r\n";
        for (int column = 0; column < columns; ++column)
        {
            if (column != 0)
                result.push_back('\t');
            result += "value";
        }
    }
    return result;
}

std::unique_ptr<ysDui::core::Control> MakeControlTree(int depth, int childrenPerNode)
{
    auto root = std::make_unique<ysDui::core::Control>();
    root->SetBounds({0, 0, 1000, 800});
    if (depth == 0)
        return root;

    for (int index = 0; index < childrenPerNode; ++index)
        root->AddChild(MakeControlTree(depth - 1, childrenPerNode));
    return root;
}

#if defined(YSDUI_PERFORMANCE_WIN32)
struct GuiResources final
{
    DWORD gdi{};
    DWORD user{};
};

GuiResources ReadGuiResources()
{
    HANDLE process = ::GetCurrentProcess();
    return {::GetGuiResources(process, GR_GDIOBJECTS), ::GetGuiResources(process, GR_USEROBJECTS)};
}

void PrintProcessMemory()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    const BOOL read = ::GetProcessMemoryInfo(
        ::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
    assert(read != FALSE);
    std::cout << "working_set_bytes=" << counters.WorkingSetSize << '\n';
    std::cout << "peak_working_set_bytes=" << counters.PeakWorkingSetSize << '\n';
    std::cout << "private_bytes=" << counters.PrivateUsage << '\n';
}
#endif

} // namespace

int main()
{
    using ysDui::controls::layout::DuiStack;
    using ysDui::controls::layout::DuiStackItem;

    DuiStack stack;
    stack.SetOrientation(ysDui::controls::layout::DuiStackOrientation::Vertical);
    for (int index = 0; index < 1000; ++index)
        stack.AddChild(std::make_unique<ysDui::core::Control>(), DuiStackItem{1, 1, {}});
    stack.Layout({0, 0, 1200, 1000});
    const long long layoutUs = MeasureMedianMicroseconds([&stack]
    {
        for (int iteration = 0; iteration < 20; ++iteration)
            stack.Layout({0, 0, 1200, 1000 + iteration});
    });

    ysDui::controls::list::DuiVirtualList list;
    BenchmarkCanvas canvas;
    list.SetRowCount(10000);
    list.SetRowHeight(24);
    list.SetRowRenderer([&canvas](ysDui::render::Canvas&, int, ysDui::core::Rect, bool, bool)
    {
        ++canvas.drawCalls;
    });
    list.Layout({0, 0, 800, 600});
    const long long virtualListUs = MeasureMedianMicroseconds([&list, &canvas]
    {
        for (int position = 0; position < 240000; position += 240)
        {
            list.SetScrollPosition(position);
            list.Paint(canvas, list.Bounds());
        }
    });
    assert(canvas.drawCalls > 10000);

    SpreadsheetBenchmarkModel sparseBindModel;
    ysDui::controls::list::DuiSpreadsheet sparseBindSpreadsheet;
    sparseBindSpreadsheet.SetWorkbookModel(&sparseBindModel);
    sparseBindSpreadsheet.Layout({0, 0, 800, 600});
    std::size_t sparseBindAllocations{};
    sparseBindModel.ResetAccessCounts();
    {
        AllocationScope allocations(sparseBindAllocations);
        sparseBindSpreadsheet.ReloadFromModel();
    }
    assert(sparseBindModel.sparseRowSizeReads == 1 && sparseBindModel.sparseColumnSizeReads == 1);
    assert(sparseBindModel.rowHeightReads == 0 && sparseBindModel.columnWidthReads == 0);
    assert(sparseBindAllocations < 100);
    const long long sparseBindUs = MeasureMedianMicroseconds([&sparseBindSpreadsheet, &sparseBindModel]
    {
        sparseBindModel.ResetAccessCounts();
        for (int iteration = 0; iteration < 1000; ++iteration)
            sparseBindSpreadsheet.ReloadFromModel();
        assert(sparseBindModel.sparseRowSizeReads == 1000 && sparseBindModel.sparseColumnSizeReads == 1000);
        assert(sparseBindModel.rowHeightReads == 0 && sparseBindModel.columnWidthReads == 0);
    });

    SpreadsheetBenchmarkModel spreadsheetModel;
    ysDui::controls::list::DuiSpreadsheet spreadsheet;
    spreadsheet.SetWorkbookModel(&spreadsheetModel);
    spreadsheet.Layout({0, 0, 800, 600});
    std::size_t spreadsheetAllocations{};
    const long long spreadsheetUs = MeasureMedianMicroseconds([&spreadsheet, &spreadsheetModel, &canvas,
                                                                 &spreadsheetAllocations]
    {
        AllocationScope allocations(spreadsheetAllocations);
        ResetSpreadsheetScroll(spreadsheet);
        spreadsheetModel.ResetAccessCounts();
        for (int frame = 0; frame < 1000; ++frame)
        {
            ysDui::core::Event event;
            event.type = ysDui::core::EventType::PointerWheel;
            event.position = {400, 300};
            event.wheelDelta = -120;
            spreadsheet.OnEvent(event);
            spreadsheet.Paint(canvas, spreadsheet.Bounds());
        }
    });
    assert(spreadsheetModel.cellTextReads <= 300'000);
    assert(spreadsheetModel.rowHeightReads == 0);
    assert(spreadsheetModel.columnWidthReads == 0);
    assert(spreadsheetAllocations < 1'000);

    spreadsheet.SetActiveCell({99999, 999});
    spreadsheet.SetFrozenPanes(99999, 999);
    assert(spreadsheet.FrozenRowCount() == 99999 && spreadsheet.FrozenColumnCount() == 999);
    std::size_t frozenSpreadsheetAllocations{};
    const long long frozenSpreadsheetUs = MeasureMedianMicroseconds(
        [&spreadsheet, &spreadsheetModel, &canvas, &frozenSpreadsheetAllocations]
    {
        AllocationScope allocations(frozenSpreadsheetAllocations);
        ResetSpreadsheetScroll(spreadsheet);
        spreadsheetModel.ResetAccessCounts();
        for (int frame = 0; frame < 1000; ++frame)
        {
            ysDui::core::Event event;
            event.type = ysDui::core::EventType::PointerWheel;
            event.position = {400, 300};
            event.wheelDelta = -120;
            spreadsheet.OnEvent(event);
            spreadsheet.Paint(canvas, spreadsheet.Bounds());
        }
    });
    assert(spreadsheetModel.cellTextReads <= 500'000);
    assert(spreadsheetModel.rowHeightReads == 0);
    assert(spreadsheetModel.columnWidthReads == 0);
    assert(frozenSpreadsheetAllocations < 1'000);

    SpreadsheetBenchmarkModel resizeModel;
    ysDui::controls::list::DuiSpreadsheet resizeSpreadsheet;
    resizeSpreadsheet.SetWorkbookModel(&resizeModel);
    resizeSpreadsheet.Layout({0, 0, 800, 600});
    ysDui::core::Event resizeDown;
    resizeDown.type = ysDui::core::EventType::PointerDown;
    resizeDown.position = {10, 48};
    assert(resizeSpreadsheet.OnEvent(resizeDown));
    const long long spreadsheetRowResizeUs = MeasureMedianMicroseconds([&resizeSpreadsheet, &resizeModel]
    {
        resizeModel.ResetAccessCounts();
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            ysDui::core::Event resizeMove;
            resizeMove.type = ysDui::core::EventType::PointerMove;
            resizeMove.position = {10, 49 + iteration % 2};
            assert(resizeSpreadsheet.OnEvent(resizeMove));
        }
        assert(resizeModel.rowHeightReads == 1000 && resizeModel.columnWidthReads == 0);
    });
    ysDui::core::Event resizeUp;
    resizeUp.type = ysDui::core::EventType::PointerUp;
    resizeUp.position = {10, 50};
    assert(resizeSpreadsheet.OnEvent(resizeUp));

    SpreadsheetBenchmarkModel batchModel;
    ysDui::controls::list::DuiSpreadsheet batchSpreadsheet;
    BenchmarkClipboard batchClipboard;
    batchClipboard.text = MakeTsv(25, 40);
    batchSpreadsheet.SetWorkbookModel(&batchModel);
    batchSpreadsheet.SetClipboard(&batchClipboard);
    batchSpreadsheet.Layout({0, 0, 800, 600});
    std::size_t batchWriteAllocations{};
    const long long spreadsheetBatchWriteUs = MeasureMedianMicroseconds(
        [&batchSpreadsheet, &batchModel, &batchWriteAllocations]
        {
            AllocationScope allocations(batchWriteAllocations);
            batchModel.ResetAccessCounts();
            ysDui::core::Event pasteEvent;
            pasteEvent.type = ysDui::core::EventType::KeyDown;
            pasteEvent.key = 'V';
            pasteEvent.modifiers = ysDui::core::modifier::Control;
            const bool pasted = batchSpreadsheet.OnEvent(pasteEvent);
            assert(pasted && batchModel.cellTextWrites == 1000);
        });

    auto tree = MakeControlTree(3, 10);
    ysDui::core::Control* hit{};
    const long long treeHitTestUs = MeasureMedianMicroseconds([&tree, &hit]
    {
        for (int iteration = 0; iteration < 10000; ++iteration)
            hit = tree->HitTest({iteration % 1000, iteration % 800});
    });
    assert(hit != nullptr);

    std::cout << "layout_1000_controls_20_iterations_us=" << layoutUs << '\n';
    std::cout << "virtual_list_10000_rows_1000_frames_us=" << virtualListUs << '\n';
    std::cout << "spreadsheet_sparse_bind_1000_reloads_us=" << sparseBindUs << '\n';
    std::cout << "spreadsheet_sparse_bind_allocations=" << sparseBindAllocations << '\n';
    std::cout << "spreadsheet_100000x1000_1000_frames_us=" << spreadsheetUs << '\n';
    std::cout << "spreadsheet_100000x1000_1000_frames_allocations=" << spreadsheetAllocations << '\n';
    std::cout << "spreadsheet_frozen_100000x1000_1000_frames_us=" << frozenSpreadsheetUs << '\n';
    std::cout << "spreadsheet_frozen_100000x1000_1000_frames_allocations="
              << frozenSpreadsheetAllocations << '\n';
    std::cout << "spreadsheet_row_resize_1000_moves_us=" << spreadsheetRowResizeUs << '\n';
    std::cout << "spreadsheet_batch_write_1000_cells_us=" << spreadsheetBatchWriteUs << '\n';
    std::cout << "spreadsheet_batch_write_1000_cells_allocations=" << batchWriteAllocations << '\n';
    std::cout << "spreadsheet_cell_reads_1000_frames=" << spreadsheetModel.cellTextReads << '\n';
    std::cout << "control_tree_1111_nodes_10000_hit_tests_us=" << treeHitTestUs << '\n';

#if defined(YSDUI_PERFORMANCE_WIN32)
    const auto renderText = []
    {
        return ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({800, 600},
            [](ysDui::render::Canvas& renderCanvas, ysDui::core::Rect)
            {
                const ysDui::render::DuiTextStyle style{{20, 20, 20, 255}, "Segoe UI", 10, false, false};
                for (int index = 0; index < 20; ++index)
                {
                    const int top = (index % 30) * 20;
                    renderCanvas.DrawText("ysDui 中文 العربية e\xCC\x81 😀", {0, top, 800, top + 20},
                                          style, ysDui::render::DuiTextAlignment::Start, false);
                }
            });
    };
    assert(renderText().has_value());
    const GuiResources resourcesBefore = ReadGuiResources();
    const long long textRenderUs = MeasureMedianMicroseconds([&renderText]
    {
        for (int iteration = 0; iteration < 5; ++iteration)
            assert(renderText().has_value());
    });
    assert(textRenderUs < 100'000);
    const GuiResources resourcesAfter = ReadGuiResources();
    assert(resourcesAfter.gdi <= resourcesBefore.gdi);
    assert(resourcesAfter.user <= resourcesBefore.user);
    std::cout << "directwrite_100_draws_us=" << textRenderUs << '\n';
    std::cout << "gdi_objects_before=" << resourcesBefore.gdi << '\n';
    std::cout << "gdi_objects_after=" << resourcesAfter.gdi << '\n';
    std::cout << "user_objects_before=" << resourcesBefore.user << '\n';
    std::cout << "user_objects_after=" << resourcesAfter.user << '\n';
    PrintProcessMemory();
#endif

    return 0;
}
