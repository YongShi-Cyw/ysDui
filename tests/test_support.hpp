/**
 * 文件名：test_support.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：集中定义平台无关回归测试共用的轻量测试替身。
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "ysDui/core/DuiHost.hpp"
#include "ysDui/core/DuiOverlayControl.hpp"
#include "ysDui/core/DuiSkin.hpp"
#include "ysDui/core/DuiAccessibility.hpp"
#include "ysDui/core/DuiMnemonic.hpp"
#include "ysDui/core/DuiAnimationClock.hpp"
#include "ysDui/core/DuiDpi.hpp"
#include "ysDui/core/DuiDateTime.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/ui/DuiDropTarget.hpp"
#include "ysDui/ui/DuiClipboard.hpp"
#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/basic/DuiCheckBox.hpp"
#include "ysDui/controls/basic/DuiChip.hpp"
#include "ysDui/controls/basic/DuiRadio.hpp"
#include "ysDui/controls/chat/DuiChatBubble.hpp"
#include "ysDui/controls/chat/DuiChatList.hpp"
#include "ysDui/controls/basic/DuiCard.hpp"
#include "ysDui/controls/basic/DuiInfoBar.hpp"
#include "ysDui/controls/basic/DuiBadge.hpp"
#include "ysDui/controls/basic/DuiAnchor.hpp"
#include "ysDui/controls/basic/DuiBreadcrumb.hpp"
#include "ysDui/controls/basic/DuiExpander.hpp"
#include "ysDui/controls/basic/DuiAvatar.hpp"
#include "ysDui/controls/basic/DuiGroupBox.hpp"
#include "ysDui/controls/basic/DuiLabel.hpp"
#include "ysDui/controls/basic/DuiSeparator.hpp"
#include "ysDui/controls/basic/DuiWatermark.hpp"
#include "ysDui/controls/basic/DuiSegmentedControl.hpp"
#include "ysDui/controls/basic/DuiStatusBar.hpp"
#include "ysDui/controls/basic/DuiToolBar.hpp"
#include "ysDui/controls/basic/DuiToast.hpp"
#include "ysDui/controls/basic/DuiToastCenter.hpp"
#include "ysDui/controls/chart/DuiLineChart.hpp"
#include "ysDui/controls/chart/DuiBarChart.hpp"
#include "ysDui/controls/chart/DuiPieChart.hpp"
#include "ysDui/controls/graph/DuiNodeEditor.hpp"
#include "ysDui/controls/graph/DuiNodeGraph.hpp"
#include "ysDui/controls/window/DuiFlyout.hpp"
#include "ysDui/controls/window/DuiTeachingTip.hpp"
#include "ysDui/controls/window/DuiGuide.hpp"
#include "ysDui/controls/window/DuiNavigationView.hpp"
#include "ysDui/controls/media/DuiImage.hpp"
#include "ysDui/controls/feedback/DuiProgressBar.hpp"
#include "ysDui/controls/feedback/DuiBusyIndicator.hpp"
#include "ysDui/controls/feedback/DuiTypingIndicator.hpp"
#include "ysDui/controls/feedback/DuiSkeleton.hpp"
#include "ysDui/controls/feedback/DuiEmojiPanel.hpp"
#include "ysDui/controls/feedback/DuiToolTip.hpp"
#include "ysDui/controls/media/DuiGif.hpp"
#include "ysDui/controls/list/DuiTab.hpp"
#include "ysDui/controls/list/DuiTabPage.hpp"
#include "ysDui/controls/list/DuiVirtualList.hpp"
#include "ysDui/controls/list/DuiListBox.hpp"
#include "ysDui/controls/list/DuiDataGrid.hpp"
#include "ysDui/controls/list/DuiSpreadsheet.hpp"
#include "ysDui/controls/list/DuiMenu.hpp"
#include "ysDui/controls/list/DuiMenuBar.hpp"
#include "ysDui/controls/list/DuiTreeView.hpp"
#include "ysDui/controls/window/DuiDialog.hpp"
#include "ysDui/controls/window/DuiMessageBox.hpp"
#include "ysDui/controls/window/DuiNativeHost.hpp"
#include "ysDui/controls/list/DuiPropertyGrid.hpp"
#include "ysDui/controls/list/DuiPagination.hpp"
#include "ysDui/controls/input/DuiSwitch.hpp"
#include "ysDui/controls/input/DuiScrollBar.hpp"
#include "ysDui/controls/input/DuiSlider.hpp"
#include "ysDui/controls/input/DuiRangeSlider.hpp"
#include "ysDui/controls/input/DuiRatingControl.hpp"
#include "ysDui/controls/input/DuiSpinBox.hpp"
#include "ysDui/controls/input/DuiHotKey.hpp"
#include "ysDui/controls/input/DuiDoubleSpinBox.hpp"
#include "ysDui/controls/input/DuiSearchBox.hpp"
#include "ysDui/controls/input/DuiPathEdit.hpp"
#include "ysDui/controls/input/DuiComboBox.hpp"
#include "ysDui/controls/input/DuiAutoSuggestBox.hpp"
#include "ysDui/controls/input/DuiColorPicker.hpp"
#include "ysDui/controls/input/DuiEditHost.hpp"
#include "ysDui/controls/input/DuiEditContextMenu.hpp"
#include "ysDui/controls/input/DuiRichEditHost.hpp"
#include "ysDui/controls/input/DuiRichDocument.hpp"
#include "ysDui/controls/input/DuiRichDocumentJson.hpp"
#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"
#include "ysDui/controls/input/DuiDateTimePicker.hpp"
#include "ysDui/controls/input/DuiMonthCalendar.hpp"
#include "ysDui/controls/DuiXmlBuilder.hpp"
#include "ysDui/controls/docking/DuiDockManager.hpp"
#include "ysDui/controls/docking/DuiDockTree.hpp"
#include "ysDui/controls/docking/DuiDockManager.hpp"
#include "ysDui/controls/layout/DuiFlow.hpp"
#include "ysDui/controls/layout/DuiDock.hpp"
#include "ysDui/controls/layout/DuiStack.hpp"
#include "ysDui/controls/layout/DuiLayout.hpp"
#include "ysDui/controls/layout/DuiUniformGrid.hpp"
#include "ysDui/controls/layout/DuiCanvas.hpp"
#include "ysDui/controls/layout/DuiSplitter.hpp"
#include "ysDui/controls/layout/DuiScrollView.hpp"
#include "ysDui/render/DuiDisplayList.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiNinePatch.hpp"
#include "ysDui/render/DuiAnimatedImage.hpp"
#include "ysDui/render/DuiFocusVisual.hpp"
#include "ysDui/render/DuiGolden.hpp"
#include "ysDui/render/DuiIconFont.hpp"
#include "ysDui/render/DuiInspector.hpp"
#include "ysDui/render/DuiSvg.hpp"
#include "ysDui/render/DuiXmlDocument.hpp"
#include "ysDui/ui/DuiHostFactory.hpp"
#include "ysDui/ui/DuiFrameHost.hpp"

namespace {
class EventControl final : public ysDui::core::Control {
public:
    bool OnEvent(const ysDui::core::Event&) override { received = true; return true; }
    bool received{};
};

class CapturingControl final : public ysDui::core::Control {
public:
    bool OnEvent(const ysDui::core::Event& event) override {
        if (event.type == ysDui::core::EventType::PointerDown) {
            SetCaptured(true);
            return true;
        }
        if (event.type == ysDui::core::EventType::PointerCancel) {
            cancelled = true;
            return true;
        }
        return false;
    }
    bool cancelled{};
};

class AccessibleControl final : public ysDui::core::Control {
public:
    explicit AccessibleControl(ysDui::core::DuiAccessibilityRole role) : role_(role) {}

protected:
    [[nodiscard]] ysDui::core::DuiAccessibilityData CreateAccessibilityData() const override
    {
        return {role_, {}, {}, {}, false, {}};
    }

private:
    ysDui::core::DuiAccessibilityRole role_;
};

class OverlayControl final : public ysDui::core::DuiOverlayControl {
public:
    void SetAlpha(double alpha) { SetOverlayAlpha(alpha); }
    [[nodiscard]] double Alpha() const { return OverlayAlpha(); }
};

class RecordingCanvas final : public ysDui::render::Canvas {
public:
    void DrawImage(const ysDui::render::DuiImage& image, ysDui::core::Rect destination) override {
        const ysDui::core::Size size = image.Size();
        DrawImage(image, {0, 0, size.width, size.height}, destination);
    }
    void DrawImageRounded(const ysDui::render::DuiImage& image, ysDui::core::Rect bounds, int radius) override {
        imageCornerRadius = TransformLength(radius);
        DrawImage(image, bounds);
    }
    void DrawImageEllipse(const ysDui::render::DuiImage& image,
                          ysDui::core::Rect bounds) override { DrawImage(image, bounds); }
    void DrawImage(const ysDui::render::DuiImage& image, ysDui::core::Rect source,
                   ysDui::core::Rect destination) override {
        lastImage = &image;
        drawnImages.push_back(&image);
        imageSource = source;
        imageDestination = TransformRect(destination);
        ++images;
    }
    [[nodiscard]] std::shared_ptr<const ysDui::render::DuiImage> Rasterize(
        ysDui::core::Size size,
        const std::function<void(ysDui::render::Canvas&, ysDui::core::Rect)>& paint) const override
    {
        if (size.Empty() || !paint)
            return {};
        // 测试替身：白底中央画一块黑，供水印白底抠图与旋转包围盒验证
        std::vector<unsigned char> pixels(
            static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) * 4, 255);
        const int left = size.width / 4;
        const int top = size.height / 4;
        const int right = size.width - left;
        const int bottom = size.height - top;
        for (int y = top; y < bottom; ++y)
        {
            for (int x = left; x < right; ++x)
            {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width)
                     + static_cast<std::size_t>(x))
                    * 4;
                pixels[offset] = 0;
                pixels[offset + 1] = 0;
                pixels[offset + 2] = 0;
            }
        }
        RecordingCanvas nested;
        paint(nested, {0, 0, size.width, size.height});
        return ysDui::render::DuiImage::CreateBgra8Premultiplied(size, std::move(pixels));
    }
    void StrokeArc(ysDui::core::Rect bounds, float, float, ysDui::core::Color color, float width) override {
        arcBounds = TransformRect(bounds);
        arcColor = color;
        arcWidth = TransformStroke(width);
        ++arcs;
    }
    void StrokePath(const ysDui::render::DuiPath& path, ysDui::core::Color, float) override {
        strokedPaths.push_back(TransformPath(path));
        ++paths;
    }
    void StrokeCubicBezier(ysDui::core::Point p0, ysDui::core::Point p1, ysDui::core::Point p2,
                           ysDui::core::Point p3, ysDui::core::Color, float) override {
        ysDui::render::DuiPath path;
        path.MoveTo(TransformPoint(p0));
        path.LineTo(TransformPoint(p1));
        path.LineTo(TransformPoint(p2));
        path.LineTo(TransformPoint(p3));
        strokedPaths.push_back(path);
        ++paths;
    }
    void StrokeRoundedRect(ysDui::core::Rect bounds, int, ysDui::core::Color color, float) override {
        bounds = TransformRect(bounds);
        strokeBounds = bounds;
        strokeColor = color;
        strokeBoundsList.push_back(bounds);
        strokeColors.push_back(color);
        ++roundedStrokes;
    }
    void FillRoundedRect(ysDui::core::Rect bounds, int radius, ysDui::core::Color color) override {
        rounded = {TransformRect(bounds), color};
        roundedRadius = TransformLength(radius);
    }
    void FillLinearGradient(ysDui::core::Rect bounds, int radius,
                            const ysDui::render::DuiLinearGradient& gradient) override {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        linearGradientBounds = bounds;
        linearGradientRadius = radius;
        linearGradients.push_back({bounds, radius});
        linearGradient = gradient;
    }
    void FillRadialGradient(ysDui::core::Rect bounds, int radius,
                            const ysDui::render::DuiRadialGradient& gradient) override {
        bounds = TransformRect(bounds);
        radius = TransformLength(radius);
        radialGradientBounds = bounds;
        radialGradientRadius = radius;
        radialGradient = gradient;
    }
    void FillEllipse(ysDui::core::Rect bounds, ysDui::core::Color color) override {
        ellipses.push_back({TransformRect(bounds), color});
    }
    void PushClip(ysDui::core::Rect bounds) override { clips.push_back(TransformRect(bounds)); }
    void PopClip() override { ++popCount; }
    void FillRect(ysDui::core::Rect bounds, ysDui::core::Color color) override {
        fills.push_back({TransformRect(bounds), color});
    }
    void FillPath(const ysDui::render::DuiPath& path, ysDui::core::Color color) override {
        filledPaths.push_back(TransformPath(path));
        pathFillColors.push_back(color);
    }
    void DrawText(std::string_view text, ysDui::core::Rect bounds,
                  const ysDui::render::DuiTextStyle& style,
                  ysDui::render::DuiTextAlignment alignment, bool wordWrap) override {
        drawnText = std::string(text);
        drawnTexts.emplace_back(text);
        drawnTextBounds.emplace_back(TransformRect(bounds));
        drawnTextStyles.push_back(TransformText(style));
        textBounds = TransformRect(bounds);
        textStyle = TransformText(style);
        textAlignment = alignment;
        wrapped = wordWrap;
    }
    ysDui::render::DuiTextMetrics MeasureText(std::string_view text, const ysDui::render::DuiTextStyle&,
                                               const ysDui::render::DuiTextMeasureOptions&) override {
        int glyphCount{};
        for (std::size_t index{}; index < text.size(); ++glyphCount) {
            const unsigned char first = static_cast<unsigned char>(text[index]);
            const std::size_t width = first < 0x80U ? 1 : first < 0xE0U ? 2 : first < 0xF0U ? 3 : 4;
            index += (std::min)(width, text.size() - index);
        }
        return {{glyphCount * 8, 10}, 1, 8};
    }
    struct Fill { ysDui::core::Rect bounds; ysDui::core::Color color; };
    struct GradientFill { ysDui::core::Rect bounds; int radius; }; // 渐变填充记录：用于校验方角补块
    std::vector<Fill> fills;
    std::vector<Fill> ellipses;
    std::vector<GradientFill> linearGradients;
    std::vector<ysDui::core::Color> pathFillColors;
    std::vector<ysDui::render::DuiPath> filledPaths;
    std::vector<ysDui::render::DuiPath> strokedPaths;
    Fill rounded{};
    ysDui::core::Rect linearGradientBounds;
    ysDui::render::DuiLinearGradient linearGradient;
    ysDui::core::Rect radialGradientBounds;
    ysDui::render::DuiRadialGradient radialGradient;
    ysDui::core::Rect strokeBounds;
    ysDui::core::Color strokeColor;
    std::vector<ysDui::core::Rect> strokeBoundsList;
    std::vector<ysDui::core::Color> strokeColors;
    int roundedRadius{};
    int linearGradientRadius{};
    int radialGradientRadius{};
    std::vector<ysDui::core::Rect> clips;
    int popCount{};
    int arcs{};
    ysDui::core::Rect arcBounds;
    ysDui::core::Color arcColor;
    float arcWidth{};
    int paths{};
    int roundedStrokes{};
    std::string drawnText;
    std::vector<std::string> drawnTexts;
    std::vector<ysDui::core::Rect> drawnTextBounds;
    std::vector<ysDui::render::DuiTextStyle> drawnTextStyles;
    ysDui::core::Rect textBounds;
    ysDui::render::DuiTextStyle textStyle;
    ysDui::render::DuiTextAlignment textAlignment{};
    bool wrapped{};
    ysDui::core::Rect imageSource;
    ysDui::core::Rect imageDestination;
    const ysDui::render::DuiImage* lastImage{};
    std::vector<const ysDui::render::DuiImage*> drawnImages;
    int images{};
    int imageCornerRadius{-1};
};

class TextInputMock final : public ysDui::ui::DuiTextInput {
public:
    void SetBounds(ysDui::core::Rect value) override
    {
        bounds = value;
        // 先拷贝再调用：回调内清空自身时避免 UAF（std::function 自毁）。
        if (boundsChanged)
        {
            const auto callback = boundsChanged;
            callback();
        }
    }
    [[nodiscard]] ysDui::core::Rect Bounds() const override { return bounds; }
    void SetVisible(bool value) override
    {
        visible = value;
        if (visibleChanged)
        {
            const auto callback = visibleChanged;
            callback(value);
        }
    }
    void SetEnabled(bool value) override { enabled = value; }
    void SetBorderVisible(bool value) override { borderVisible = value; }
    void SetOptions(const ysDui::ui::DuiTextInputOptions& value) override { options = value; }
    void SetPlaceholder(std::string value) override { placeholder = std::move(value); }
    void SetText(std::string value) override {
        text = std::move(value);
        selection = {text.size(), text.size()};
        if (changed)
        {
            const auto callback = changed;
            callback();
        }
    }
    [[nodiscard]] std::string Text() const override { return text; }
    [[nodiscard]] std::size_t CaretPosition() const override { return selection.end; }
    [[nodiscard]] bool CaretVisible() const override { return caretVisible; }
    [[nodiscard]] ysDui::ui::DuiTextSelection TextSelection() const override { return selection; }
    void Focus() override
    {
        focused = true;
        if (focusRequested)
        {
            const auto callback = focusRequested;
            callback();
        }
    }
    void BeginSelection(ysDui::core::Point position) override
    {
        FocusAt(position);
        selectionBegin = position;
        ++selectionBeginCount;
    }
    void UpdateSelection(ysDui::core::Point position) override
    {
        selectionUpdate = position;
        ++selectionUpdateCount;
    }
    void EndSelection() override { ++selectionEndCount; }
    void SelectWordAt(ysDui::core::Point position) override
    {
        selectionWord = position;
        ++selectionWordCount;
    }
    void SetChangedHandler(std::function<void()> handler) override
    {
        changed = std::move(handler);
        if (changedHandlerChanged)
        {
            const auto callback = changedHandlerChanged;
            callback();
        }
    }
    void SetFocusLostHandler(std::function<void()> handler) override
    {
        focusLost = std::move(handler);
        if (focusLostHandlerChanged)
        {
            const auto callback = focusLostHandlerChanged;
            callback();
        }
    }
    void SetSubmitHandler(std::function<void()> handler) override { submitted = std::move(handler); }
    void SetCancelHandler(std::function<void()> handler) override { cancelled = std::move(handler); }
    void LoseFocus()
    {
        if (focusLost)
        {
            const auto callback = focusLost;
            callback();
        }
    }
    void Submit()
    {
        if (submitted)
        {
            const auto callback = submitted;
            callback();
        }
    }
    void Cancel()
    {
        if (cancelled)
        {
            const auto callback = cancelled;
            callback();
        }
    }

    ysDui::core::Rect bounds;
    std::string text;
    ysDui::ui::DuiTextSelection selection;
    ysDui::core::Point selectionBegin;
    ysDui::core::Point selectionUpdate;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void()> submitted;
    std::function<void()> cancelled;
    std::function<void()> boundsChanged;
    std::function<void(bool)> visibleChanged;
    std::function<void()> changedHandlerChanged;
    std::function<void()> focusLostHandlerChanged;
    std::function<void()> focusRequested;
    bool visible{};
    bool caretVisible{true};
    bool enabled{};
    bool borderVisible{};
    bool focused{};
    int selectionBeginCount{};
    int selectionUpdateCount{};
    int selectionEndCount{};
    int selectionWordCount{};
    ysDui::core::Point selectionWord;
    std::string placeholder;
    ysDui::ui::DuiTextInputOptions options;
};

class RichTextInputMock final : public ysDui::ui::DuiRichTextInput {
public:
    void SetBounds(ysDui::core::Rect value) override { bounds = value; }
    [[nodiscard]] ysDui::core::Rect Bounds() const override { return bounds; }
    void SetVisible(bool value) override { visible = value; }
    void SetEnabled(bool value) override { enabled = value; }
    void SetBorderVisible(bool value) override { borderVisible = value; }
    void SetOptions(const ysDui::ui::DuiTextInputOptions& value) override { options = value; }
    void SetPlaceholder(std::string value) override { placeholder = std::move(value); }
    void SetText(std::string value) override { text = std::move(value); selection = {0, 0}; NotifyChanged(); }
    [[nodiscard]] std::string Text() const override { return text; }
    void Focus() override { focused = true; }
    void SetChangedHandler(std::function<void()> handler) override { changed = std::move(handler); }
    void SetFocusLostHandler(std::function<void()> handler) override { focusLost = std::move(handler); }
    void SetSelection(ysDui::ui::DuiTextRange value) override { selection = value; }
    [[nodiscard]] ysDui::ui::DuiTextRange Selection() const override { return selection; }
    void SelectAll() override { selection = {0, static_cast<int>(text.size())}; }
    void ReplaceSelection(std::string value) override {
        const int start = selection.start < 0 ? 0 : selection.start;
        const int end = selection.end < start ? start : selection.end;
        text.replace(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start), value);
        selection = {start + static_cast<int>(value.size()), start + static_cast<int>(value.size())};
        NotifyChanged();
    }
    void AppendText(std::string value) override { selection = {static_cast<int>(text.size()), static_cast<int>(text.size())}; ReplaceSelection(std::move(value)); }
    [[nodiscard]] bool CanUndo() const override { return canUndo; }
    void Undo() override { undone = true; }
    void Cut() override { cut = true; }
    void Copy() override { copied = true; }
    void Paste() override { pasted = true; }
    void ClearSelection() override { ReplaceSelection({}); }
    void SetSelectionFormat(ysDui::ui::DuiRichTextFormat value) override { format = value; }
    void SetAutomaticLinkDetection(bool value) override { automaticLinkDetection = value; }
    void SetLinkActivatedHandler(std::function<void(std::string)> handler) override { linkActivated = std::move(handler); }
    [[nodiscard]] bool InsertImage(const ysDui::render::DuiImage&, ysDui::core::Size) override { return false; }
    void InsertQuoteBlock(std::string sender, std::string body) override { quoteSender = std::move(sender); quoteBody = std::move(body); }
    void InsertFileCard(std::string name, std::uint64_t size) override { fileName = std::move(name); fileSize = size; }
    void EmitLink(std::string link) { if (linkActivated) linkActivated(std::move(link)); }
    void NotifyChanged() { if (changed) changed(); }
    ysDui::core::Rect bounds;
    ysDui::ui::DuiTextInputOptions options;
    ysDui::ui::DuiTextRange selection;
    ysDui::ui::DuiRichTextFormat format;
    std::string text;
    std::string placeholder;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void(std::string)> linkActivated;
    bool visible{};
    bool enabled{};
    bool borderVisible{};
    bool focused{};
    bool canUndo{true};
    bool automaticLinkDetection{};
    bool undone{};
    bool cut{};
    bool copied{};
    bool pasted{};
    std::string quoteSender;
    std::string quoteBody;
    std::string fileName;
    std::uint64_t fileSize{};
};

class PathPickerMock final : public ysDui::ui::DuiPathPicker {
public:
    [[nodiscard]] ysDui::ui::DuiPathPickerResult Browse(ysDui::ui::DuiPathPickerMode value,
                                                                std::string_view initial) override {
        mode = value;
        initialPath = initial;
        return result;
    }

    ysDui::ui::DuiPathPickerMode mode{};
    std::string initialPath;
    ysDui::ui::DuiPathPickerResult result;
};

class ColorChooserMock final : public ysDui::ui::DuiColorChooser {
public:
    [[nodiscard]] ysDui::ui::DuiColorChooserResult Choose(ysDui::core::Color value) override {
        initial = value;
        return result;
    }
    ysDui::core::Color initial;
    ysDui::ui::DuiColorChooserResult result;
};

class PopupHostMock final : public ysDui::ui::IPopupHost {
public:
    bool Show(const ysDui::ui::DuiPopupOptions& value, std::unique_ptr<ysDui::core::Control> valueContent,
              LayoutHandler valueLayout, PaintHandler valuePaint) override {
        options = value;
        content = std::move(valueContent);
        layout = std::move(valueLayout);
        paint = std::move(valuePaint);
        if (layout) layout({0, 0, options.size.width, options.size.height});
        visible = true;
        return true;
    }
    void SetOptions(const ysDui::ui::DuiPopupOptions& value) override {
        options = value;
        if (layout) layout({0, 0, options.size.width, options.size.height});
    }
    void Hide() override { visible = false; if (dismissed) dismissed(); }
    void RequestHide() override { requestedHide = true; visible = false; if (dismissed) dismissed(); }
    void RequestMinimize() override { requestedMinimize = true; }
    void RequestMove() override { requestedMove = true; }
    [[nodiscard]] bool Visible() const override { return visible; }
    [[nodiscard]] ysDui::ui::HostRef Reference() const override { return {}; }
    void SetDismissedHandler(std::function<void()> handler) override { dismissed = std::move(handler); }
    bool Dispatch(const ysDui::core::Event& event) { return content && content->OnEvent(event); }
    ysDui::ui::DuiPopupOptions options;
    std::unique_ptr<ysDui::core::Control> content;
    LayoutHandler layout;
    PaintHandler paint;
    std::function<void()> dismissed;
    bool visible{};
    bool requestedHide{};
    bool requestedMinimize{};
    bool requestedMove{};
};

class FrameHostMock final : public ysDui::ui::IFrameHost {
public:
    bool Show(const ysDui::ui::DuiFrameOptions& frameOptions,
              std::unique_ptr<ysDui::core::Control> frameContent,
              LayoutHandler layout, PaintHandler paint) override {
        if (!frameContent || frameOptions.size.Empty()) return false;
        options = frameOptions;
        content = std::move(frameContent);
        layoutHandler = std::move(layout);
        paintHandler = std::move(paint);
        shown = true;
        visible = true;
        if (layoutHandler) layoutHandler({0, 0, frameOptions.size.width, frameOptions.size.height});
        return true;
    }
    void SetOptions(const ysDui::ui::DuiFrameOptions& frameOptions) override { options = frameOptions; }
    void SetContent(std::unique_ptr<ysDui::core::Control> frameContent, LayoutHandler layout,
                    PaintHandler paint) override {
        content = std::move(frameContent);
        layoutHandler = std::move(layout);
        paintHandler = std::move(paint);
    }
    void Close() override { closedByHost = true; closed = true; visible = false; }
    void RequestClose() override { requestedClose = true; }
    [[nodiscard]] std::unique_ptr<ysDui::core::Control> DetachContent() override {
        layoutHandler = {};
        paintHandler = {};
        detachCount++;
        return std::move(content);
    }
    [[nodiscard]] bool Visible() const override { return visible; }
    [[nodiscard]] ysDui::ui::HostRef Reference() const override { return {}; }
    void SetCaptionButtonHandler(std::function<void(int)> handler) override {
        captionButtonHandler = std::move(handler);
    }
    void SetClosedHandler(std::function<void()> handler) override { closedHandler = std::move(handler); }

    ysDui::ui::DuiFrameOptions options;
    std::unique_ptr<ysDui::core::Control> content;
    LayoutHandler layoutHandler;
    PaintHandler paintHandler;
    std::function<void()> closedHandler;
    std::function<void(int)> captionButtonHandler;
    bool shown{};
    bool visible{};
    bool closed{};
    bool requestedClose{};  // RequestClose（异步关窗）已调用
    bool closedByHost{};    // Close（宿主同步销毁）已调用
    int detachCount{};
};

class UiHostFactoryMock final : public ysDui::ui::IUiHostFactory {
public:
    [[nodiscard]] std::unique_ptr<ysDui::ui::IEmbeddedHost> CreateEmbeddedHost(ysDui::ui::HostRef) override { return {}; }
    [[nodiscard]] std::unique_ptr<ysDui::ui::IFrameHost> CreateFrameHost() override {
        auto result = std::make_unique<FrameHostMock>();
        frame = result.get();
        return result;
    }
    [[nodiscard]] std::unique_ptr<ysDui::ui::DuiNativeViewHost> CreateNativeViewHost(ysDui::ui::HostRef) override { return {}; }
    [[nodiscard]] std::unique_ptr<ysDui::ui::IPopupHost> CreatePopupHost(ysDui::ui::HostRef) override {
        auto result = std::make_unique<PopupHostMock>();
        popup = result.get();
        return result;
    }
    [[nodiscard]] std::unique_ptr<ysDui::ui::ILayeredHost> CreateLayeredHost(ysDui::ui::HostRef) override { return {}; }

    PopupHostMock* popup{};
    FrameHostMock* frame{};
};

class DateTimeSourceMock final : public ysDui::ui::DuiDateTimeSource {
public:
    [[nodiscard]] ysDui::core::DateTime LocalNow() const override { return now; }
    ysDui::core::DateTime now{2024, 2, 29, 12, 30, 0};
};

class FontRegistryMock final : public ysDui::render::DuiFontRegistry {
public:
    bool RegisterFile(std::string path) override {
        file = std::move(path);
        return registerFileResult;
    }
    bool RegisterMemory(std::vector<std::uint8_t> bytes) override {
        memory = std::move(bytes);
        return registerMemoryResult;
    }
    [[nodiscard]] bool HasFamily(std::string_view family) const override {
        return std::find(families.begin(), families.end(), family) != families.end();
    }

    std::string file;
    std::vector<std::uint8_t> memory;
    std::vector<std::string> families;
    bool registerFileResult{true};
    bool registerMemoryResult{true};
};

class ClipboardMock final : public ysDui::ui::DuiClipboard {
public:
    bool SetText(std::string value) override {
        text = std::move(value);
        return true;
    }
    [[nodiscard]] std::optional<std::string> GetText() const override { return text; }

    std::string text;
};
}
