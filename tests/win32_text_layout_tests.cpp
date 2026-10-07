/**
 * 文件名：win32_text_layout_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：验证 DirectWrite 对复杂 UTF-8 文本的测量、字体回退、换行和离屏绘制。
 */
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif

#include "ysDui/platform/win32/DuiWin32FontRegistry.hpp"
#include "ysDui/platform/win32/DuiWin32OffscreenRenderer.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/render/DuiIconFont.hpp"
#include "DuiWin32Canvas.hpp"
#include "DuiWin32PaintBuffer.hpp"

namespace {

constexpr int CanvasWidth = 360;
constexpr int RowHeight = 32;

bool RowContainsInk(const ysDui::render::DuiPixelBuffer& pixels, int row)
{
    const auto& bytes = pixels.Bytes();
    const int firstY = row * RowHeight;
    const int lastY = (std::min)(pixels.Size().height, firstY + RowHeight);
    for (int y = firstY; y < lastY; ++y)
    {
        for (int x = 0; x < pixels.Size().width; ++x)
        {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * pixels.Size().width + x) * 4;
            if (bytes[offset] < 245 || bytes[offset + 1] < 245 || bytes[offset + 2] < 245)
                return true;
        }
    }
    return false;
}

bool ContainsInk(const ysDui::render::DuiPixelBuffer& pixels)
{
    const auto& bytes = pixels.Bytes();
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4)
    {
        if (bytes[offset] < 245 || bytes[offset + 1] < 245 || bytes[offset + 2] < 245)
            return true;
    }
    return false;
}

class SpreadsheetTextModel final : public ysDui::controls::list::IDuiWorkbookModel
{
public:
    explicit SpreadsheetTextModel(const std::array<std::string_view, 7>& values) : values_(values) {}

    [[nodiscard]] int WorksheetCount() const override { return 1; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId WorksheetIdAt(int) const override { return 1; }
    [[nodiscard]] std::string WorksheetName(ysDui::controls::list::DuiWorksheetId) const override { return "Sheet1"; }
    bool RenameWorksheet(ysDui::controls::list::DuiWorksheetId, std::string) override { return true; }
    [[nodiscard]] ysDui::controls::list::DuiWorksheetId AddWorksheet(std::string) override { return 0; }
    bool RemoveWorksheet(ysDui::controls::list::DuiWorksheetId) override { return false; }
    [[nodiscard]] int RowCount(ysDui::controls::list::DuiWorksheetId) const override
    {
        return static_cast<int>(values_.size());
    }
    [[nodiscard]] int ColumnCount(ysDui::controls::list::DuiWorksheetId) const override { return 1; }
    [[nodiscard]] std::string CellText(ysDui::controls::list::DuiWorksheetId,
                                       ysDui::controls::list::DuiCellAddress cell) const override
    {
        return cell.row >= 0 && cell.row < static_cast<int>(values_.size()) && cell.column == 0
            ? std::string(values_[static_cast<std::size_t>(cell.row)]) : std::string{};
    }
    bool SetCellText(ysDui::controls::list::DuiWorksheetId,
                     ysDui::controls::list::DuiCellAddress, std::string) override { return true; }
    [[nodiscard]] int RowHeight(ysDui::controls::list::DuiWorksheetId, int) const override { return 24; }
    [[nodiscard]] int ColumnWidth(ysDui::controls::list::DuiWorksheetId, int) const override { return 180; }
    bool SetRowHeight(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }
    bool SetColumnWidth(ysDui::controls::list::DuiWorksheetId, int, int) override { return true; }

private:
    const std::array<std::string_view, 7>& values_;
};

bool SpreadsheetRowContainsInk(HDC context, int row)
{
    constexpr int Dpi = 144;
    const ysDui::core::DuiDpiScale scale(Dpi);
    const int left = scale.Scale(42 + 4);
    const int right = scale.Scale(42 + 180 - 4);
    const int top = scale.Scale(24 + row * 24 + 2);
    const int bottom = scale.Scale(24 + (row + 1) * 24 - 2);
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            if (::GetPixel(context, x, y) != RGB(255, 255, 255))
                return true;
    return false;
}

} // namespace

int main()
{
    constexpr std::array<std::string_view, 7> Samples{
        "DirectWrite text",
        "中文文本回归",
        "العربية",
        "e\xCC\x81" "cole",
        "A\xF0\x9F\x98\x80" "B",
        "abc العربية 123",
        "עברית English"};

    std::array<ysDui::render::DuiTextMetrics, Samples.size()> metrics{};
    ysDui::render::DuiTextMetrics wrapped{};
    const auto rendered = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render(
        {CanvasWidth, static_cast<int>(Samples.size()) * RowHeight},
        [&metrics, &wrapped](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 255, 255, 255});
            ysDui::render::DuiTextStyle style;
            style.family = "Font That Does Not Exist, Segoe UI, Microsoft YaHei UI";
            style.pointSize = 12;
            for (std::size_t index = 0; index < Samples.size(); ++index)
            {
                metrics[index] = canvas.MeasureText(Samples[index], style, {});
                const int top = static_cast<int>(index) * RowHeight;
                canvas.DrawText(Samples[index], {4, top, CanvasWidth - 4, top + RowHeight},
                                style, ysDui::render::DuiTextAlignment::Start, false);
            }
            wrapped = canvas.MeasureText(
                "English 中文 العربية emoji \xF0\x9F\x98\x80 mixed direction text",
                style, {90, true});
        });

    assert(rendered.has_value());
    for (std::size_t index = 0; index < Samples.size(); ++index)
    {
        assert(metrics[index].size.width > 0);
        assert(metrics[index].size.height > 0);
        assert(metrics[index].lineCount == 1);
        assert(metrics[index].baseline > 0);
        if (!RowContainsInk(*rendered, static_cast<int>(index)))
        {
            std::cerr << "DirectWrite produced no pixels for sample " << index << '\n';
            return 10 + static_cast<int>(index);
        }
    }
    assert(wrapped.size.width > 0 && wrapped.size.width <= 90);
    assert(wrapped.size.height > 0);
    assert(wrapped.lineCount > 1);
    assert(wrapped.baseline > 0);

    auto renderIcon = [](std::string_view family, ysDui::render::DuiTextMetrics& iconMetrics)
    {
        return ysDui::platform::win32::DuiWin32OffscreenRenderer::Render(
            {64, 64},
            [family, &iconMetrics](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
            {
                canvas.FillRect(bounds, {255, 255, 255, 255});
                ysDui::render::DuiTextStyle iconStyle;
                iconStyle.family = family;
                iconStyle.pointSize = 20;
                iconStyle.color = {20, 80, 180, 255};
                constexpr std::string_view HomeIcon = "\xEF\x80\x95";
                iconMetrics = canvas.MeasureText(HomeIcon, iconStyle, {});
                canvas.DrawText(HomeIcon, bounds, iconStyle,
                                ysDui::render::DuiTextAlignment::Center, false);
            });
    };

    ysDui::render::DuiTextMetrics fileIconMetrics{};
    std::optional<ysDui::render::DuiPixelBuffer> fileIcon;
    {
        ysDui::platform::win32::Win32FontRegistry fontRegistry;
        ysDui::render::DuiIconFont iconFont(fontRegistry);
        assert(iconFont.LoadFromFile("fonts/fa-solid-900.ttf"));
        fileIcon = renderIcon(iconFont.Family(), fileIconMetrics);
    }
    assert(fileIcon.has_value());
    assert(fileIconMetrics.size.width > 0 && fileIconMetrics.size.height > 0);
    assert(ContainsInk(*fileIcon));

    std::ifstream fontFile("fonts/fa-solid-900.ttf", std::ios::binary);
    assert(fontFile);
    std::vector<std::uint8_t> fontBytes{std::istreambuf_iterator<char>(fontFile),
                                        std::istreambuf_iterator<char>()};
    ysDui::platform::win32::Win32FontRegistry memoryFontRegistry;
    ysDui::render::DuiIconFont memoryIconFont(memoryFontRegistry);
    assert(memoryIconFont.LoadFromMemory(std::move(fontBytes)));
    ysDui::render::DuiTextMetrics memoryIconMetrics{};
    const auto memoryIcon = renderIcon(memoryIconFont.Family(), memoryIconMetrics);
    assert(memoryIcon.has_value());
    assert(memoryIconMetrics.size.width == fileIconMetrics.size.width);
    assert(memoryIconMetrics.size.height == fileIconMetrics.size.height);
    assert(memoryIcon->Bytes() == fileIcon->Bytes());

    HDC screen = ::GetDC(nullptr);
    assert(screen != nullptr);

    SpreadsheetTextModel spreadsheetModel(Samples);
    ysDui::controls::list::DuiSpreadsheet spreadsheet;
    spreadsheet.SetWorkbookModel(&spreadsheetModel);
    spreadsheet.Layout({0, 0, 320, 280});
    ysDui::platform::win32::Win32PaintBuffer spreadsheetBuffer;
    assert(spreadsheetBuffer.Prepare(reinterpret_cast<std::uintptr_t>(screen), 480, 420));
    spreadsheetBuffer.Clear();
    auto spreadsheetCanvas = ysDui::platform::win32::CreateWin32Canvas(
        spreadsheetBuffer.Context(), 144);
    spreadsheet.Paint(*spreadsheetCanvas, spreadsheet.Bounds());
    spreadsheetCanvas.reset();
    const HDC spreadsheetContext = reinterpret_cast<HDC>(spreadsheetBuffer.Context());
    for (int row = 0; row < static_cast<int>(Samples.size()); ++row)
        assert(SpreadsheetRowContainsInk(spreadsheetContext, row));

    ysDui::platform::win32::Win32PaintBuffer paintBuffer;
    assert(paintBuffer.Prepare(reinterpret_cast<std::uintptr_t>(screen), 160, RowHeight));
    paintBuffer.Clear();
    auto canvas = ysDui::platform::win32::CreateWin32Canvas(paintBuffer.Context());
    ysDui::render::DuiTextStyle style;
    style.pointSize = 12;
    canvas->DrawText("DirectWrite DDB", {0, 0, 160, RowHeight}, style,
                     ysDui::render::DuiTextAlignment::Start, false);
    canvas.reset();
    bool ddbContainsInk{};
    const HDC bufferContext = reinterpret_cast<HDC>(paintBuffer.Context());
    for (int y = 0; y < RowHeight && !ddbContainsInk; ++y)
    {
        for (int x = 0; x < 160; ++x)
        {
            if (::GetPixel(bufferContext, x, y) != RGB(255, 255, 255))
            {
                ddbContainsInk = true;
                break;
            }
        }
    }
    assert(ddbContainsInk);

    // 文字颜色的 alpha 必须生效：约 25% 黑不能渲染成不透明黑
    // （回归：Win32 文字后端曾把颜色压成 COLORREF，丢弃 alpha）
    const auto translucent = ysDui::platform::win32::DuiWin32OffscreenRenderer::Render({160, RowHeight},
        [](ysDui::render::Canvas& canvas, ysDui::core::Rect bounds)
        {
            canvas.FillRect(bounds, {255, 255, 255, 255});
            ysDui::render::DuiTextStyle translucentStyle;
            translucentStyle.pointSize = 12;
            translucentStyle.color = {0, 0, 0, 64};
            canvas.DrawText("MEMO MEMO", bounds, translucentStyle,
                            ysDui::render::DuiTextAlignment::Start, false);
        });
    assert(translucent.has_value());
    int darkestChannel = 255;
    {
        const auto& bytes = translucent->Bytes();
        for (std::size_t offset = 0; offset + 3 < bytes.size(); offset += 4)
            darkestChannel = (std::min)(darkestChannel, static_cast<int>(bytes[offset + 1]));
    }
    // 丢弃 alpha 会得到近 0；正确混合后字形核心约 255 * (1 - 64/255) ≈ 191
    assert(darkestChannel > 100 && darkestChannel < 245);

    assert(::ReleaseDC(nullptr, screen) == 1);
    return 0;
}
