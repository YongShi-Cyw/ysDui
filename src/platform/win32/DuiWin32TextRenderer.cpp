/**
 * 文件名：DuiWin32TextRenderer.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-30
 * 用途：使用 DirectWrite 统一 Win32 文本 shaping、换行、测量和绘制。
 */
#include "DuiWin32TextRenderer.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <dwrite.h>
#include <dwrite_3.h>
#include <windows.h>
#include <wrl/client.h>

#ifdef DrawText
#undef DrawText
#endif

#include "DuiWin32Utf8.hpp"
#include "DuiWin32RegisteredFonts.hpp"

namespace ysDui::platform::win32 {
namespace {

using Microsoft::WRL::ComPtr;

constexpr float PointsToDips = 96.0F / 72.0F;
constexpr float UnboundedLayout = 1'000'000.0F;

class TextRenderer final : public IDWriteTextRenderer
{
public:
    TextRenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* renderingParams,
                 core::Color color)
        : target_(target), renderingParams_(renderingParams),
          color_(RGB(color.red, color.green, color.blue))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (id == __uuidof(IUnknown) || id == __uuidof(IDWritePixelSnapping)
            || id == __uuidof(IDWriteTextRenderer))
        {
            *result = static_cast<IDWriteTextRenderer*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG references = --references_;
        if (references == 0) delete this;
        return references;
    }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override
    {
        if (disabled == nullptr) return E_POINTER;
        *disabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override
    {
        if (transform == nullptr) return E_POINTER;
        return target_->GetCurrentTransform(transform);
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override
    {
        if (pixelsPerDip == nullptr) return E_POINTER;
        *pixelsPerDip = target_->GetPixelsPerDip();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineOriginX,
                                           FLOAT baselineOriginY,
                                           DWRITE_MEASURING_MODE measuringMode,
                                           const DWRITE_GLYPH_RUN* glyphRun,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION*,
                                           IUnknown*) override
    {
        return target_->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode,
                                     glyphRun, renderingParams_, color_, nullptr);
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT baselineOriginX,
                                            FLOAT baselineOriginY,
                                            const DWRITE_UNDERLINE* underline,
                                            IUnknown*) override
    {
        if (underline == nullptr) return E_INVALIDARG;
        const FLOAT pixelsPerDip = target_->GetPixelsPerDip();
        const LONG left = static_cast<LONG>(std::floor(baselineOriginX * pixelsPerDip));
        const LONG top = static_cast<LONG>(std::floor(
            (baselineOriginY + underline->offset) * pixelsPerDip));
        const RECT bounds{
            left,
            top,
            static_cast<LONG>(std::ceil((baselineOriginX + underline->width) * pixelsPerDip)),
            static_cast<LONG>(std::ceil(
                (baselineOriginY + underline->offset + underline->thickness) * pixelsPerDip))};
        const HBRUSH brush = ::CreateSolidBrush(color_);
        if (brush == nullptr) return E_OUTOFMEMORY;
        ::FillRect(target_->GetMemoryDC(), &bounds, brush);
        ::DeleteObject(brush);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT baselineOriginX,
                                                FLOAT baselineOriginY,
                                                const DWRITE_STRIKETHROUGH* strikethrough,
                                                IUnknown*) override
    {
        if (strikethrough == nullptr) return E_INVALIDARG;
        const FLOAT pixelsPerDip = target_->GetPixelsPerDip();
        const LONG left = static_cast<LONG>(std::floor(baselineOriginX * pixelsPerDip));
        const LONG top = static_cast<LONG>(std::floor(
            (baselineOriginY + strikethrough->offset) * pixelsPerDip));
        const RECT bounds{
            left,
            top,
            static_cast<LONG>(std::ceil(
                (baselineOriginX + strikethrough->width) * pixelsPerDip)),
            static_cast<LONG>(std::ceil(
                (baselineOriginY + strikethrough->offset + strikethrough->thickness)
                * pixelsPerDip))};
        const HBRUSH brush = ::CreateSolidBrush(color_);
        if (brush == nullptr) return E_OUTOFMEMORY;
        ::FillRect(target_->GetMemoryDC(), &bounds, brush);
        ::DeleteObject(brush);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT,
                                               IDWriteInlineObject*, BOOL, BOOL,
                                               IUnknown*) override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
    IDWriteBitmapRenderTarget* target_{};
    IDWriteRenderingParams* renderingParams_{};
    COLORREF color_{};
};

[[nodiscard]] std::wstring TrimmedFamily(std::string_view family)
{
    std::size_t first{};
    std::size_t last = family.size();
    while (first < last && (family[first] == ' ' || family[first] == '\t')) ++first;
    while (last > first && (family[last - 1] == ' ' || family[last - 1] == '\t')) --last;
    return detail::Utf8ToWide(family.substr(first, last - first));
}

} // namespace

struct DuiWin32TextRenderer::Impl final
{
    struct FontSelection final
    {
        std::wstring family;
        ComPtr<IDWriteFontCollection> collection;
    };

    struct FormatEntry final
    {
        std::string families;
        int pointSize{};
        bool bold{};
        bool italic{};
        bool wordWrap{};
        render::DuiTextAlignment alignment{render::DuiTextAlignment::Start};
        ComPtr<IDWriteTextFormat> format;
    };

    explicit Impl(std::uintptr_t deviceContext, int dpiValue)
        : context(reinterpret_cast<HDC>(deviceContext)), scale(dpiValue)
    {
        if (FAILED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                         __uuidof(IDWriteFactory),
                                         reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
        {
            return;
        }
        (void)factory->GetGdiInterop(&gdiInterop);
        (void)factory->CreateRenderingParams(&renderingParams);
        (void)factory->GetSystemFontCollection(&systemFonts, FALSE);
    }

    ~Impl()
    {
        privateFonts.Reset();
        releaseMemoryFontLoader();
    }

    [[nodiscard]] bool Ready() const
    {
        return context != nullptr && factory != nullptr && gdiInterop != nullptr
            && renderingParams != nullptr;
    }

    [[nodiscard]] ComPtr<IDWriteFontCollection> PrivateFontCollection() const
    {
        const registered_fonts::Snapshot snapshot = registered_fonts::FontSnapshot();
        if (snapshot.generation == privateFontGeneration) return privateFonts;
        privateFontGeneration = snapshot.generation;
        formats.clear();
        privateFonts.Reset();
        releaseMemoryFontLoader();
        if (snapshot.files.empty() && snapshot.memoryFiles.empty()) return {};

        ComPtr<IDWriteFactory3> factory3;
        ComPtr<IDWriteFontSetBuilder> baseBuilder;
        ComPtr<IDWriteFontSetBuilder1> builder;
        if (FAILED(factory.As(&factory3))
            || FAILED(factory3->CreateFontSetBuilder(&baseBuilder))
            || FAILED(baseBuilder.As(&builder)))
            return {};
        bool added{};
        for (const auto& path : snapshot.files)
        {
            ComPtr<IDWriteFontFile> fontFile;
            if (SUCCEEDED(factory->CreateFontFileReference(path.c_str(), nullptr, &fontFile)))
            {
                if (SUCCEEDED(builder->AddFontFile(fontFile.Get()))) added = true;
            }
        }
        if (!snapshot.memoryFiles.empty())
        {
            ComPtr<IDWriteFactory5> factory5;
            if (SUCCEEDED(factory.As(&factory5))
                && SUCCEEDED(factory5->CreateInMemoryFontFileLoader(&memoryFontLoader))
                && SUCCEEDED(factory->RegisterFontFileLoader(memoryFontLoader.Get())))
            {
                memoryFontLoaderRegistered = true;
                for (const auto& bytes : snapshot.memoryFiles)
                {
                    ComPtr<IDWriteFontFile> fontFile;
                    if (bytes->size() <= (std::numeric_limits<UINT32>::max)()
                        && SUCCEEDED(memoryFontLoader->CreateInMemoryFontFileReference(
                            factory.Get(), bytes->data(), static_cast<UINT32>(bytes->size()),
                            nullptr, &fontFile))
                        && SUCCEEDED(builder->AddFontFile(fontFile.Get())))
                    {
                        added = true;
                    }
                }
            }
        }
        if (!added) return {};
        ComPtr<IDWriteFontSet> fontSet;
        ComPtr<IDWriteFontCollection1> collection;
        if (FAILED(builder->CreateFontSet(&fontSet))
            || FAILED(factory3->CreateFontCollectionFromFontSet(fontSet.Get(), &collection)))
        {
            return {};
        }
        privateFonts = collection;
        return privateFonts;
    }

    void releaseMemoryFontLoader() const
    {
        if (memoryFontLoaderRegistered)
            (void)factory->UnregisterFontFileLoader(memoryFontLoader.Get());
        memoryFontLoaderRegistered = false;
        memoryFontLoader.Reset();
    }

    [[nodiscard]] FontSelection SelectFont(std::string_view families) const
    {
        if (systemFonts == nullptr)
            return {L"Segoe UI", {}};
        const ComPtr<IDWriteFontCollection> privateCollection = PrivateFontCollection();

        std::size_t begin{};
        while (begin < families.size())
        {
            const std::size_t delimiter = families.find(',', begin);
            const std::size_t end = delimiter == std::string_view::npos ? families.size() : delimiter;
            const std::wstring family = TrimmedFamily(families.substr(begin, end - begin));
            if (!family.empty())
            {
                UINT32 index{};
                BOOL exists{};
                if (privateCollection != nullptr
                    && SUCCEEDED(privateCollection->FindFamilyName(family.c_str(), &index, &exists))
                    && exists)
                {
                    return {family, privateCollection};
                }
                exists = FALSE;
                if (SUCCEEDED(systemFonts->FindFamilyName(
                        family.c_str(), &index, &exists)) && exists)
                    return {family, systemFonts};
            }
            if (delimiter == std::string_view::npos) break;
            begin = delimiter + 1;
        }
        return {L"Segoe UI", systemFonts};
    }

    [[nodiscard]] ComPtr<IDWriteTextFormat> TextFormat(
        const render::DuiTextStyle& style, bool wordWrap,
        render::DuiTextAlignment alignment) const
    {
        (void)PrivateFontCollection();
        const auto current = std::find_if(formats.begin(), formats.end(),
            [&style, wordWrap, alignment](const FormatEntry& entry)
            {
                return entry.families == style.family && entry.pointSize == style.pointSize
                    && entry.bold == style.bold && entry.italic == style.italic
                    && entry.wordWrap == wordWrap && entry.alignment == alignment;
            });
        if (current != formats.end())
            return current->format;

        const FontSelection selection = SelectFont(style.family);
        ComPtr<IDWriteTextFormat> format;
        const float fontSize = static_cast<float>((std::max)(1, style.pointSize)) * PointsToDips;
        if (FAILED(factory->CreateTextFormat(
                selection.family.c_str(), selection.collection.Get(),
                style.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, fontSize,
                L"", &format)))
        {
            return {};
        }
        format->SetWordWrapping(wordWrap ? DWRITE_WORD_WRAPPING_WRAP
                                         : DWRITE_WORD_WRAPPING_NO_WRAP);
        switch (alignment)
        {
        case render::DuiTextAlignment::Center:
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            break;
        case render::DuiTextAlignment::End:
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            break;
        case render::DuiTextAlignment::Start:
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            break;
        }
        format->SetParagraphAlignment(wordWrap ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR
                                               : DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        formats.push_back({style.family, style.pointSize, style.bold, style.italic, wordWrap, alignment, format});
        return format;
    }

    [[nodiscard]] ComPtr<IDWriteTextLayout> CreateLayout(
        std::string_view text, const render::DuiTextStyle& style,
        float width, float height, bool wordWrap,
        render::DuiTextAlignment alignment) const
    {
        if (!Ready()) return {};
        const ComPtr<IDWriteTextFormat> format = TextFormat(style, wordWrap, alignment);
        if (format == nullptr) return {};

        const std::wstring nativeText = detail::Utf8ToWide(text);
        ComPtr<IDWriteTextLayout> layout;
        if (FAILED(factory->CreateTextLayout(nativeText.data(),
                                             static_cast<UINT32>(nativeText.size()),
                                             format.Get(), width, height, &layout)))
        {
            return {};
        }
        if (style.underline && !nativeText.empty())
        {
            const DWRITE_TEXT_RANGE range{0, static_cast<UINT32>(nativeText.size())};
            (void)layout->SetUnderline(TRUE, range);
        }
        return layout;
    }

    HDC context{};
    core::DuiDpiScale scale;
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteGdiInterop> gdiInterop;
    ComPtr<IDWriteRenderingParams> renderingParams;
    ComPtr<IDWriteFontCollection> systemFonts;
    ComPtr<IDWriteBitmapRenderTarget> bitmapTarget;
    core::Size bitmapTargetSize;
    mutable std::size_t privateFontGeneration{(std::numeric_limits<std::size_t>::max)()};
    mutable ComPtr<IDWriteFontCollection> privateFonts;
    mutable ComPtr<IDWriteInMemoryFontFileLoader> memoryFontLoader;
    mutable bool memoryFontLoaderRegistered{};
    mutable std::vector<FormatEntry> formats;
};

DuiWin32TextRenderer::DuiWin32TextRenderer(std::uintptr_t deviceContext, int dpi)
    : renderer_(std::make_unique<Impl>(deviceContext, dpi))
{
}

DuiWin32TextRenderer::~DuiWin32TextRenderer() = default;
DuiWin32TextRenderer::DuiWin32TextRenderer(DuiWin32TextRenderer&&) noexcept = default;
DuiWin32TextRenderer& DuiWin32TextRenderer::operator=(DuiWin32TextRenderer&&) noexcept = default;

void DuiWin32TextRenderer::DrawText(std::string_view text, core::Rect bounds,
                                    const render::DuiTextStyle& style,
                                    render::DuiTextAlignment alignment, bool wordWrap)
{
    if (text.empty() || bounds.Empty() || style.color.alpha == 0 || !renderer_->Ready()) return;
    const core::Rect physicalBounds = renderer_->scale.Scale(bounds);
    if (renderer_->bitmapTarget == nullptr)
    {
        if (FAILED(renderer_->gdiInterop->CreateBitmapRenderTarget(
                renderer_->context, physicalBounds.Width(), physicalBounds.Height(),
                &renderer_->bitmapTarget)))
        {
            return;
        }
        renderer_->bitmapTargetSize = {physicalBounds.Width(), physicalBounds.Height()};
    }
    else if (physicalBounds.Width() > renderer_->bitmapTargetSize.width
             || physicalBounds.Height() > renderer_->bitmapTargetSize.height)
    {
        const core::Size targetSize{
            (std::max)(physicalBounds.Width(), renderer_->bitmapTargetSize.width),
            (std::max)(physicalBounds.Height(), renderer_->bitmapTargetSize.height)};
        if (FAILED(renderer_->bitmapTarget->Resize(targetSize.width, targetSize.height)))
            return;
        renderer_->bitmapTargetSize = targetSize;
    }

    renderer_->bitmapTarget->SetPixelsPerDip(static_cast<float>(renderer_->scale.Factor()));
    HDC memory = renderer_->bitmapTarget->GetMemoryDC();
    if (!::BitBlt(memory, 0, 0, physicalBounds.Width(), physicalBounds.Height(),
                  renderer_->context, physicalBounds.left, physicalBounds.top, SRCCOPY))
    {
        return;
    }

    auto layout = renderer_->CreateLayout(text, style,
                                           static_cast<float>(bounds.Width()),
                                           static_cast<float>(bounds.Height()),
                                           wordWrap, alignment);
    if (layout == nullptr) return;
    auto* textRenderer = new TextRenderer(renderer_->bitmapTarget.Get(),
                                          renderer_->renderingParams.Get(), style.color);
    const HRESULT result = layout->Draw(nullptr, textRenderer, 0.0F, 0.0F);
    textRenderer->Release();
    if (FAILED(result)) return;

    // 文字最终合成：alpha 255 直接按位拷贝；否则把整块以常量 alpha 混回目标。
    // 记忆 DC 是目标区域的精确副本，只有字形（及下划线/删除线）改动了它，
    // 因此对整块做一次 SourceConstantAlpha 混合，等价于「只以该 alpha 绘制文字」：
    // 背景像素 src==dst，混合后保持不变；字形像素得到正确的 alpha 混合结果。
    if (style.color.alpha == 255)
    {
        (void)::BitBlt(renderer_->context, physicalBounds.left, physicalBounds.top,
                       physicalBounds.Width(), physicalBounds.Height(), memory, 0, 0, SRCCOPY);
        return;
    }
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = style.color.alpha;
    blend.AlphaFormat = 0;
    (void)::AlphaBlend(renderer_->context, physicalBounds.left, physicalBounds.top,
                       physicalBounds.Width(), physicalBounds.Height(), memory, 0, 0,
                       physicalBounds.Width(), physicalBounds.Height(), blend);
}

render::DuiTextMetrics DuiWin32TextRenderer::MeasureText(
    std::string_view text, const render::DuiTextStyle& style,
    const render::DuiTextMeasureOptions& options) const
{
    if (text.empty() || !renderer_->Ready()) return {};
    const float width = options.maximumWidth > 0
        ? static_cast<float>(options.maximumWidth) : UnboundedLayout;
    auto layout = renderer_->CreateLayout(text, style, width, UnboundedLayout,
                                           options.wordWrap,
                                           render::DuiTextAlignment::Start);
    if (layout == nullptr) return {};

    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics))) return {};
    UINT32 lineCount{};
    (void)layout->GetLineMetrics(nullptr, 0, &lineCount);
    if (lineCount == 0) return {};
    std::vector<DWRITE_LINE_METRICS> lines(lineCount);
    if (FAILED(layout->GetLineMetrics(lines.data(), lineCount, &lineCount))) return {};

    const float measuredWidth = options.wordWrap
        ? (std::min)(metrics.widthIncludingTrailingWhitespace, width)
        : metrics.widthIncludingTrailingWhitespace;
    return {{static_cast<int>(std::ceil(measuredWidth)),
             static_cast<int>(std::ceil(metrics.height))},
            static_cast<int>(lineCount),
            static_cast<int>(std::ceil(lines.front().baseline))};
}

} // namespace ysDui::platform::win32
