/**
 * 文件名：controls_markdown_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：DuiMarkdownView 解析、消毒、流式围栏与交互回归测试。
 */
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <cstdint>

#include "DuiMarkdownAst.hpp"
#include "DuiMarkdownHighlight.hpp"
#include "ysDui/controls/content/DuiMarkdownView.hpp"
#include "ysDui/controls/DuiXmlBuilder.hpp"
#include "ysDui/controls/layout/DuiScrollView.hpp"
#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"

using ysDui::controls::content::DuiMarkdownView;
using ysDui::controls::content::DuiMarkdownLinkTarget;
using ysDui::controls::content::DuiMarkdownAppearance;
using ysDui::controls::layout::DuiScrollView;
namespace md = ysDui::controls::content::detail;

namespace {

struct MemoryClipboard final : ysDui::ui::DuiClipboard
{
    bool SetText(std::string text) override
    {
        stored = std::move(text);
        return true;
    }
    [[nodiscard]] std::optional<std::string> GetText() const override { return stored; }
    std::string stored;
};

} // namespace

int main()
{
    {
        DuiMarkdownView view;
        view.SetContent("# Title\n\nHello **bold** and *em*.\n\n- a\n- b\n");
        view.Layout({0, 0, 400, 800});
        const auto size = view.ContentSize();
        assert(size.width > 0 && size.height > 0);
        assert(view.Sanitize());
        assert(view.CodeCopyable());
        assert(view.EnableTable());
    }

    {
        DuiMarkdownView view;
        bool rendered = false;
        view.SetRenderedHandler([&] { rendered = true; });
        view.SetContent("paragraph");
        view.Layout({0, 0, 320, 200});
        (void)view.ContentSize();
        assert(rendered);
    }

    {
        // 未闭合代码围栏：流式预补全后不应把后续段落吞掉（有可布局内容）
        DuiMarkdownView view;
        view.SetStreaming(true);
        view.SetStreamThrottleMs(0);
        view.SetContent("before\n\n```cpp\nint x = 1;\n\nafter fence text");
        view.Layout({0, 0, 400, 600});
        assert(view.ContentSize().height > 20);
        view.SetStreaming(false);
        view.SetContent("before\n\n```cpp\nint x = 1;\n```\n\nafter");
        view.Layout({0, 0, 400, 600});
        assert(view.ContentSize().height > 20);
    }

    {
        // 危险链接应被剥离：点击不会命中 javascript:
        DuiMarkdownView view;
        view.SetSanitize(true);
        view.SetContent("[bad](javascript:alert(1))\n\n[ok](https://example.com)");
        view.Layout({0, 0, 400, 200});
        std::string activated;
        view.SetLinkActivatedHandler([&](std::string_view href) { activated = std::string(href); });

        RecordingCanvas canvas;
        view.Paint(canvas, view.Bounds());
        bool foundExample = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("ok") != std::string::npos || text.find("example") != std::string::npos)
                foundExample = true;
        }
        assert(foundExample);

        ysDui::core::Event down;
        down.type = ysDui::core::EventType::PointerDown;
        ysDui::core::Event up;
        up.type = ysDui::core::EventType::PointerUp;
        for (int y = view.Bounds().top; y < view.Bounds().bottom; y += 4)
        {
            for (int x = view.Bounds().left; x < view.Bounds().right; x += 8)
            {
                down.position = up.position = {x, y};
                view.OnEvent(down);
                view.OnEvent(up);
                if (!activated.empty())
                    break;
            }
            if (!activated.empty())
                break;
        }
        assert(activated == "https://example.com");
        assert(activated.find("javascript") == std::string::npos);
    }

    {
        DuiMarkdownView view;
        MemoryClipboard clipboard;
        view.SetClipboard(&clipboard);
        view.SetCodeCopyable(true);
        view.SetCodeHighlight(true);
        view.SetContent("```cpp\nint v = 42;\n```\n");
        view.Layout({0, 0, 400, 200});
        std::string copiedLang;
        view.SetCodeCopyHandler([&](std::string_view, std::string_view lang)
        {
            copiedLang = std::string(lang);
        });

        RecordingCanvas canvas;
        view.Paint(canvas, view.Bounds());

        ysDui::core::Event down;
        down.type = ysDui::core::EventType::PointerDown;
        ysDui::core::Event up;
        up.type = ysDui::core::EventType::PointerUp;
        bool hitCopy = false;
        for (int y = view.Bounds().top; y < view.Bounds().bottom; y += 2)
        {
            for (int x = view.Bounds().right - 60; x < view.Bounds().right; ++x)
            {
                down.position = up.position = {x, y};
                view.OnEvent(down);
                if (view.OnEvent(up) && !copiedLang.empty())
                {
                    hitCopy = true;
                    break;
                }
            }
            if (hitCopy)
                break;
        }
        assert(hitCopy);
        assert(copiedLang == "cpp");
        assert(clipboard.stored.find("int v = 42") != std::string::npos);
    }

    {
        DuiMarkdownView view;
        view.SetEnableTable(true);
        view.SetContent("| A | B |\n| --- | --- |\n| 1 | 2 |\n");
        view.Layout({0, 0, 400, 200});
        assert(view.ContentSize().height > 10);
        view.SetEnableTable(false);
        view.SetContent("| A | B |\n| --- | --- |\n| 1 | 2 |\n");
        view.Layout({0, 0, 400, 200});
        assert(view.ContentSize().height >= 0);
    }

    {
        ysDui::controls::DuiXmlBuilder builder;
        auto built = builder.BuildWithResult(
            "<markdownview content=\"# Hi\" sanitize=\"true\" code-copyable=\"true\" enable-table=\"true\"/>");
        assert(built.control);
        auto* view = dynamic_cast<DuiMarkdownView*>(built.control.get());
        assert(view != nullptr);
        assert(view->Content() == "# Hi");
        assert(view->Sanitize());
        assert(view->CodeCopyable());
        assert(view->EnableTable());
    }

    {
        DuiMarkdownView view;
        view.SetLinkTarget(DuiMarkdownLinkTarget::None);
        assert(view.LinkTarget() == DuiMarkdownLinkTarget::None);
        view.SetStreamingCursor(false);
        assert(!view.StreamingCursor());
        view.SetStreamThrottleMs(50);
        assert(view.StreamThrottleMs() == 50);
    }

    {
        // 回归：Layout 回退测量后 Paint 必须用 Canvas 重测，避免 run 矩形偏窄裁切字形
        DuiMarkdownView view;
        view.SetContent("快速排序是一种高效的分治排序算法，平均时间复杂度 `O(n log n)`。\n\n"
                        "## 核心思路\n\n"
                        "1. 选择一个**基准值**（pivot）\n");
        view.Layout({0, 0, 360, 800});
        const int approxHeight = view.ContentSize().height;
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 360, 800});
        assert(!canvas.drawnTexts.empty());
        bool foundHeading = false;
        bool foundPivot = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("核心思路") != std::string::npos)
                foundHeading = true;
            if (text.find("基准") != std::string::npos || text.find("pivot") != std::string::npos)
                foundPivot = true;
        }
        assert(foundHeading);
        assert(foundPivot);
        assert(view.ContentSize().height >= approxHeight / 2);
    }

    {
        // 对齐 imgui_markdown：窄宽度下英文按词边界换行，长代码行软折行增高
        DuiMarkdownView view;
        view.SetContent("word alpha beta gamma delta epsilon zeta\n\n"
                        "```js\n"
                        "function quickSort(arr) { return [...quickSort(left), ...mid, ...quickSort(right)]; }\n"
                        "```\n");
        view.Layout({0, 0, 220, 900});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 220, 900});
        bool sawAlpha = false;
        bool sawBeta = false;
        bool sawFunction = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("alpha") != std::string::npos)
                sawAlpha = true;
            if (text.find("beta") != std::string::npos)
                sawBeta = true;
            if (text.find("function") != std::string::npos || text.find("quickSort") != std::string::npos)
                sawFunction = true;
        }
        assert(sawAlpha);
        assert(sawBeta);
        assert(sawFunction);
        assert(view.ContentSize().height > 80);
    }

    {
        // 浅色 Appearance：代码块表面应变浅（非 Dark+ 深底）
        DuiMarkdownView view;
        view.SetAppearance(DuiMarkdownAppearance::Light);
        view.SetCodeHighlight(true);
        view.SetContent("```cpp\nint x = 1;\n```\n");
        view.Layout({0, 0, 400, 300});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 300});
        // 最后一次圆角填充应为代码头栏浅色底（约 #eaeef2），而非 Dark+ #2d2d2d
        assert(canvas.rounded.color.red >= 220);
        assert(canvas.rounded.color.green >= 220);
        assert(canvas.rounded.color.blue >= 220);
    }

    {
        // 图片占位：绘制 [图] alt，并可命中 src 链接
        DuiMarkdownView view;
        view.SetContent("![示意图](https://example.com/a.png)\n");
        view.Layout({0, 0, 400, 300});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 300});
        bool sawPlaceholder = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("[图]") != std::string::npos && text.find("示意图") != std::string::npos)
                sawPlaceholder = true;
        }
        assert(sawPlaceholder);

        std::string activated;
        view.SetLinkActivatedHandler([&](std::string_view href) { activated = std::string(href); });
        ysDui::core::Event down;
        down.type = ysDui::core::EventType::PointerDown;
        ysDui::core::Event up;
        up.type = ysDui::core::EventType::PointerUp;
        for (int y = view.Bounds().top; y < view.Bounds().bottom && activated.empty(); y += 4)
        {
            for (int x = view.Bounds().left; x < view.Bounds().right; x += 8)
            {
                down.position = up.position = {x, y};
                view.OnEvent(down);
                view.OnEvent(up);
                if (!activated.empty())
                    break;
            }
        }
        assert(activated == "https://example.com/a.png");
    }

    {
        // 表格列宽：短列不应与超长列等分到极端挤压（内容驱动分配后高度仍合理）
        DuiMarkdownView view;
        view.SetEnableTable(true);
        view.SetContent("| A | LongColumnHeaderName |\n| --- | --- |\n| 1 | value |\n");
        view.Layout({0, 0, 420, 300});
        assert(view.ContentSize().height > 20);
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 420, 300});
        bool sawLong = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("LongColumn") != std::string::npos)
                sawLong = true;
        }
        assert(sawLong);
    }

    {
        // Paint 零拷贝回归：多次绘制不依赖布局拷贝，内容仍可见
        DuiMarkdownView view;
        view.SetContent("# ZeroCopy\n\nHello **world** and `code`.\n");
        view.Layout({0, 0, 400, 300});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 300});
        view.Paint(canvas, {10, 10, 200, 150});
        bool saw = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("ZeroCopy") != std::string::npos || text.find("Hello") != std::string::npos
                || text.find("world") != std::string::npos)
                saw = true;
        }
        assert(saw);
        assert(view.ContentSize().height > 10);
    }

    {
        // 表格样式：表头底色 + 网格线；列对齐（左/中/右）
        DuiMarkdownView view;
        view.SetAppearance(DuiMarkdownAppearance::Light);
        view.SetEnableTable(true);
        view.SetContent("| 左对齐 | 居中对齐 | 右对齐 |\n"
                        "| :--- | :---: | ---: |\n"
                        "| 内容 A | 内容 B | 内容 C |\n"
                        "| 内容 D | 内容 E | 内容 F |\n");
        view.Layout({0, 0, 480, 280});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 480, 280});
        assert(view.ContentSize().height > 40);
        bool sawHeaderFill = false;
        int gridLines = 0;
        for (const auto& fill : canvas.fills)
        {
            if (fill.color.red >= 240 && fill.color.green >= 240 && fill.color.blue >= 240
                && fill.color.alpha == 255 && fill.bounds.Height() >= 20)
                sawHeaderFill = true;
            if (fill.color.red >= 200 && fill.color.red <= 240
                && (fill.bounds.Height() <= 2 || fill.bounds.Width() <= 2))
                ++gridLines;
        }
        assert(sawHeaderFill);
        assert(gridLines >= 4);
        bool sawLeft = false;
        bool sawCenter = false;
        bool sawRight = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("内容 A") != std::string::npos || text.find("左对齐") != std::string::npos)
                sawLeft = true;
            if (text.find("内容 B") != std::string::npos || text.find("居中") != std::string::npos)
                sawCenter = true;
            if (text.find("内容 C") != std::string::npos || text.find("右对齐") != std::string::npos)
                sawRight = true;
        }
        assert(sawLeft && sawCenter && sawRight);
    }

    {
        // 划选 + Ctrl+C / SelectAll
        DuiMarkdownView view;
        MemoryClipboard clipboard;
        view.SetClipboard(&clipboard);
        view.SetSelectable(true);
        view.SetContent("Hello **World** and more");
        view.Layout({0, 0, 400, 200});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 200});
        view.SelectAll();
        assert(!view.SelectedText().empty());
        assert(view.SelectedText().find("Hello") != std::string::npos);

        ysDui::core::Event key;
        key.type = ysDui::core::EventType::KeyDown;
        key.modifiers = ysDui::core::modifier::Control;
        key.key = 'C';
        view.SetFocused(true);
        assert(view.OnEvent(key));
        assert(clipboard.stored.find("Hello") != std::string::npos);
        view.ClearSelection();
        assert(view.SelectedText().empty());
    }

    {
        // 图片加载回调：有图时 DrawImage，不再画 [图] 占位文案
        std::vector<unsigned char> pixels(16 * 16 * 4, 255);
        auto image = ysDui::render::DuiImage::CreateBgra8Premultiplied({16, 16}, std::move(pixels));
        assert(image != nullptr);
        DuiMarkdownView view;
        view.SetImageProvider([&](std::string_view) { return image; });
        view.SetContent("![icon](https://example.com/i.png)\n");
        view.Layout({0, 0, 400, 300});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 300});
        assert(canvas.images >= 1 || canvas.lastImage != nullptr);
        bool sawPlaceholder = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find("[图]") != std::string::npos)
                sawPlaceholder = true;
        }
        assert(!sawPlaceholder);
    }

    {
        // 宽表：内容宽度应大于窄视口（供 Shift+滚轮横向滚动）
        DuiMarkdownView view;
        view.SetEnableTable(true);
        view.SetContent(
            "| ColA | VeryLongColumnHeaderThatNeedsSpace | AnotherWide |\n"
            "| --- | --- | --- |\n"
            "| 1 | abcdefghijklmnopqrstuvwxyz0123456789 | xyz |\n");
        view.Layout({0, 0, 180, 400});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 180, 400});
        assert(view.ContentSize().width > 180);
    }

    {
        // LaTeX 公式：$...$ / $$...$$ / ```math
        DuiMarkdownView view;
        view.SetContent("复杂度 $O(n\\log n)$ 与\n\n$$\\frac{a}{b}+\\sqrt{x}$$\n\n"
                        "```math\nE=mc^2\n```\n");
        view.Layout({0, 0, 480, 600});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 480, 600});
        bool sawO = false;
        bool sawE = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find('O') != std::string::npos || text.find("log") != std::string::npos)
                sawO = true;
            if (text.find('E') != std::string::npos || text.find("mc") != std::string::npos)
                sawE = true;
        }
        assert(sawO);
        assert(sawE);
        assert(view.ContentSize().height > 40);
    }

    {
        // 宽表：Shift+滚轮 / 方向键 / 中键拖动横向滚动
        DuiMarkdownView view;
        view.SetEnableTable(true);
        view.SetContent(
            "| ColA | VeryLongColumnHeaderThatNeedsSpace | AnotherWide |\n"
            "| --- | --- | --- |\n"
            "| 1 | abcdefghijklmnopqrstuvwxyz0123456789 | xyz |\n");
        view.Layout({0, 0, 180, 400});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 180, 400});
        assert(view.ContentSize().width > 180);
        assert(view.ScrollOffsetX() == 0);

        ysDui::core::Event wheel;
        wheel.type = ysDui::core::EventType::PointerWheel;
        wheel.position = {90, 200};
        wheel.modifiers = ysDui::core::modifier::Shift;
        wheel.wheelDelta = -80;
        assert(view.OnEvent(wheel));
        assert(view.ScrollOffsetX() > 0);
        const int afterWheel = view.ScrollOffsetX();

        view.SetFocused(true);
        ysDui::core::Event home;
        home.type = ysDui::core::EventType::KeyDown;
        home.key = ysDui::core::key::Home;
        assert(view.OnEvent(home));
        assert(view.ScrollOffsetX() == 0);

        ysDui::core::Event right;
        right.type = ysDui::core::EventType::KeyDown;
        right.key = ysDui::core::key::Right;
        assert(view.OnEvent(right));
        assert(view.ScrollOffsetX() > 0);

        ysDui::core::Event midDown;
        midDown.type = ysDui::core::EventType::PointerDown;
        midDown.button = ysDui::core::PointerButton::Middle;
        midDown.position = {90, 200};
        assert(view.OnEvent(midDown));
        ysDui::core::Event midMove;
        midMove.type = ysDui::core::EventType::PointerMove;
        midMove.position = {40, 200};
        assert(view.OnEvent(midMove));
        assert(view.ScrollOffsetX() >= afterWheel || view.ScrollOffsetX() > 0);
        ysDui::core::Event midUp;
        midUp.type = ysDui::core::EventType::PointerUp;
        midUp.position = {40, 200};
        assert(view.OnEvent(midUp));
    }

    {
        // 双击选词
        DuiMarkdownView view;
        view.SetSelectable(true);
        view.SetContent("Hello World again");
        view.Layout({0, 0, 400, 200});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 200});

        ysDui::core::Event dbl;
        dbl.type = ysDui::core::EventType::PointerDoubleClick;
        dbl.button = ysDui::core::PointerButton::Primary;
        dbl.position = {20, 12};
        assert(view.OnEvent(dbl));
        const std::string selected = view.SelectedText();
        assert(!selected.empty());
        assert(selected.find(' ') == std::string::npos);
    }

    {
        // 图片短缓存：同一 src 多次布局只回调一次
        std::vector<unsigned char> pixels(16 * 16 * 4, 200);
        auto image = ysDui::render::DuiImage::CreateBgra8Premultiplied({16, 16}, std::move(pixels));
        int calls = 0;
        DuiMarkdownView view;
        view.SetImageProvider([&](std::string_view)
        {
            ++calls;
            return image;
        });
        view.SetContent("![a](https://example.com/i.png)\n");
        view.Layout({0, 0, 400, 300});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 300});
        view.Layout({0, 0, 400, 300});
        view.Paint(canvas, {0, 0, 400, 300});
        assert(calls == 1);
    }

    {
        // 无障碍：只读 + Value/Selection 模式
        DuiMarkdownView view;
        view.SetSelectable(true);
        view.SetContent("# Title\n\nBody text");
        view.Layout({0, 0, 320, 200});
        const auto a11y = view.Accessibility();
        assert(a11y.role == ysDui::core::DuiAccessibilityRole::Text);
        assert(a11y.readOnly);
        assert(a11y.keyboardFocusable);
        assert(ysDui::core::HasAccessibilityPattern(a11y.patterns,
                                                    ysDui::core::DuiAccessibilityPattern::Value));
        assert(ysDui::core::HasAccessibilityPattern(a11y.patterns,
                                                    ysDui::core::DuiAccessibilityPattern::Selection));
        assert(!a11y.value.empty());
    }

    {
        // 异步本地图片：解码完成后不再绘制 [图] 占位
        std::vector<unsigned char> pixels(8 * 8 * 4, 180);
        auto image = ysDui::render::DuiImage::CreateBgra8Premultiplied({8, 8}, std::move(pixels));
        assert(image != nullptr);
        ysDui::controls::media::DuiAsyncImageLoader loader;
        loader.SetDecoder([&](const std::string&) { return image; });
        ysDui::core::AnimationClock clock;
        DuiMarkdownView view;
        view.SetAnimationClock(&clock);
        view.SetAsyncImageLoader(&loader);
        view.SetContent("![icon](demo_async.png)\n");
        view.Layout({0, 0, 400, 300});
        bool ready = false;
        for (int i = 0; i < 50 && !ready; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            clock.Advance(16);
            view.Layout({0, 0, 400, 300});
            RecordingCanvas canvas;
            view.Paint(canvas, {0, 0, 400, 300});
            if (canvas.images >= 1 || canvas.lastImage != nullptr)
                ready = true;
        }
        assert(ready);
        RecordingCanvas finalCanvas;
        view.Paint(finalCanvas, {0, 0, 400, 300});
        bool sawPlaceholder = false;
        for (const auto& text : finalCanvas.drawnTexts)
        {
            if (text.find("[图]") != std::string::npos)
                sawPlaceholder = true;
        }
        assert(!sawPlaceholder);
    }

    {
        // 扩展公式：\binom / \overline / \text / 希腊字母
        DuiMarkdownView view;
        view.SetContent("$$\\binom{n}{k}+\\overline{AB}+\\text{Hi}+\\alpha\\in\\Omega$$\n");
        view.Layout({0, 0, 520, 400});
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 520, 400});
        bool sawParen = false;
        bool sawHi = false;
        bool sawGreek = false;
        for (const auto& text : canvas.drawnTexts)
        {
            if (text.find('(') != std::string::npos || text.find(')') != std::string::npos)
                sawParen = true;
            if (text.find("Hi") != std::string::npos)
                sawHi = true;
            if (text.find("α") != std::string::npos || text.find("Ω") != std::string::npos)
                sawGreek = true;
        }
        assert(sawParen);
        assert(sawHi);
        assert(sawGreek);
        assert(view.ContentSize().height > 20);
    }

    {
        // 无障碍：首个标题作 name，description 含大纲与链接
        DuiMarkdownView view;
        view.SetContent("# 总览\n\n见 [文档](https://example.com/docs)。\n\n## 细节\n");
        view.Layout({0, 0, 400, 400});
        const auto a11y = view.Accessibility();
        assert(a11y.name == "总览");
        assert(a11y.description.find("标题大纲") != std::string::npos);
        assert(a11y.description.find("H1 总览") != std::string::npos);
        assert(a11y.description.find("H2 细节") != std::string::npos);
        assert(a11y.description.find("https://example.com/docs") != std::string::npos);

        const auto outline = view.GetOutline();
        assert(outline.size() == 2);
        assert(outline[0].level == 1);
        assert(outline[0].text == "总览");
        assert(outline[1].level == 2);
        assert(outline[1].text == "细节");
        assert(outline[1].y >= outline[0].y);
    }

    {
        // ScrollToOutline：经祖先 DuiScrollView 跳转
        DuiScrollView scroll;
        auto md = std::make_unique<DuiMarkdownView>();
        md->SetContent("# A\n\nparagraph one\n\n## B\n\nparagraph two longer\n\n### C\n\nend\n");
        DuiMarkdownView* mdRaw = md.get();
        scroll.SetContent(std::move(md));
        scroll.Layout({0, 0, 400, 160});
        constexpr int kBar = 17;
        mdRaw->Layout({0, 0, 400 - kBar, 8});
        const int contentH = mdRaw->ContentSize().height;
        scroll.SetContentSize({400 - kBar, (std::max)(contentH, 1)});
        scroll.Layout({0, 0, 400, 160});

        const auto outline = mdRaw->GetOutline();
        assert(outline.size() >= 2);
        assert(mdRaw->ScrollToOutline(1));
        assert(scroll.ScrollPosition() == outline[1].y);

        int requested = -1;
        mdRaw->SetScrollRequestHandler([&](int y) { requested = y; });
        assert(mdRaw->ScrollToY(42));
        assert(requested == 42);
        mdRaw->SetScrollRequestHandler({});
        assert(!mdRaw->ScrollToOutline(999));
    }

    {
        // 解析后块指纹已缓存，可供前缀复用直接比较
        md::ParseOptions options;
        md::MarkdownDocument doc = md::ParseMarkdown("# A\n\npara\n\n## B\n", options, nullptr);
        assert(doc.blockFingerprints.size() == doc.blocks.size());
        assert(!doc.blockFingerprints.empty());
        assert(md::CountReusablePrefixBlocks(doc, doc.blockFingerprints) == doc.blocks.size());
        std::vector<std::uint64_t> older = doc.blockFingerprints;
        older.pop_back();
        assert(md::CountReusablePrefixBlocks(doc, older) == older.size());
    }

    {
        // 流式追加：稳定前缀复用后内容高度应单调不减且可绘制
        DuiMarkdownView view;
        view.SetStreaming(true);
        view.SetStreamThrottleMs(0);
        view.SetStreamingCursor(false);
        view.SetContent("# A\n\npara one\n");
        view.Layout({0, 0, 400, 600});
        const int h1 = view.ContentSize().height;
        assert(h1 > 0);
        view.SetContent("# A\n\npara one\n\n## B\n\npara two more text\n");
        view.Layout({0, 0, 400, 600});
        const int h2 = view.ContentSize().height;
        assert(h2 >= h1);
        RecordingCanvas canvas;
        view.Paint(canvas, {0, 0, 400, 200});
        assert(!canvas.drawnTexts.empty());
    }

    {
        // 布局前缀复用：稳定块的 Y/run 几何不变；source 被 move 清空
        RecordingCanvas measurer;
        ysDui::core::DuiTheme theme;
        ysDui::render::DuiTextStyle base;
        base.pointSize = 14;
        base.family = "Segoe UI, Microsoft YaHei UI, Arial";
        const md::MarkdownPalette palette = md::MarkdownPalette::Make(true);
        md::ParseOptions options;

        md::MarkdownDocument doc1 = md::ParseMarkdown("# A\n\npara one\n", options, nullptr);
        md::MarkdownLayout layout1 =
            md::LayoutMarkdown(doc1, measurer, 400, base, true, true, theme, false, palette);
        assert(layout1.bands.size() >= 2);
        const md::LaidBand prefixBand = layout1.bands[0];
        std::vector<ysDui::core::Rect> prefixRuns;
        for (std::size_t i = prefixBand.runBegin; i < prefixBand.runEnd; ++i)
            prefixRuns.push_back(layout1.runs[i].bounds);

        std::vector<std::uint64_t> fingerprints;
        fingerprints.reserve(doc1.blocks.size());
        for (const md::BlockNode& block : doc1.blocks)
            fingerprints.push_back(md::FingerprintBlock(block));

        md::MarkdownDocument doc2 =
            md::ParseMarkdown("# A\n\npara one\n\n## B\n\npara two more text\n", options, nullptr);
        const std::size_t reusable = md::CountReusablePrefixBlocks(doc2, fingerprints);
        assert(reusable >= 2);

        md::LayoutReusePrefix reuse{&layout1, reusable};
        md::MarkdownLayout layout2 = md::LayoutMarkdown(doc2, measurer, 400, base, true, true, theme,
                                                         false, palette, {}, &reuse);
        assert(layout2.bands.size() > reusable);
        assert(layout2.bands[0].y0 == prefixBand.y0);
        assert(layout2.bands[0].y1 == prefixBand.y1);
        assert(layout2.bands[0].runEnd - layout2.bands[0].runBegin == prefixRuns.size());
        for (std::size_t i = 0; i < prefixRuns.size(); ++i)
            assert(layout2.runs[layout2.bands[0].runBegin + i].bounds == prefixRuns[i]);
        // move 后源布局应为空（或至少不再持有 runs）
        assert(layout1.runs.empty());
        assert(layout1.bands.empty());
    }

    {
        // 外观切换必须使布局失效：深/浅色代码块填充应不同
        DuiMarkdownView view;
        view.SetAppearance(DuiMarkdownAppearance::Dark);
        view.SetContent("```cpp\nint x = 1;\n```\n");
        view.Layout({0, 0, 400, 240});
        RecordingCanvas darkCanvas;
        view.Paint(darkCanvas, {0, 0, 400, 240});
        view.SetAppearance(DuiMarkdownAppearance::Light);
        view.Layout({0, 0, 400, 240});
        RecordingCanvas lightCanvas;
        view.Paint(lightCanvas, {0, 0, 400, 240});
        assert(!darkCanvas.fills.empty());
        assert(!lightCanvas.fills.empty());
        bool colorDiffered = false;
        for (const auto& darkFill : darkCanvas.fills)
        {
            for (const auto& lightFill : lightCanvas.fills)
            {
                if (darkFill.bounds.left == lightFill.bounds.left
                    && darkFill.bounds.top == lightFill.bounds.top
                    && darkFill.bounds.Width() == lightFill.bounds.Width()
                    && (darkFill.color.red != lightFill.color.red
                        || darkFill.color.green != lightFill.color.green
                        || darkFill.color.blue != lightFill.color.blue))
                {
                    colorDiffered = true;
                    break;
                }
            }
            if (colorDiffered)
                break;
        }
        assert(colorDiffered);
    }

    {
        // 加载仓库 AGENTS.md（大文档）：应能布局绘制，不崩溃
        namespace fs = std::filesystem;
        fs::path agents;
        {
            fs::path cursor = fs::current_path();
            for (int i = 0; i < 8 && agents.empty(); ++i)
            {
                const fs::path candidate = cursor / "AGENTS.md";
                if (fs::exists(candidate))
                    agents = candidate;
                else if (cursor.has_parent_path() && cursor != cursor.parent_path())
                    cursor = cursor.parent_path();
                else
                    break;
            }
        }
        if (!agents.empty())
        {
            std::ifstream input(agents, std::ios::binary);
            assert(static_cast<bool>(input));
            std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            assert(text.size() > 1000);
            if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB
                && static_cast<unsigned char>(text[2]) == 0xBF)
                text.erase(0, 3);

            DuiMarkdownView view;
            view.SetEnableTable(true);
            view.SetCodeHighlight(true);
            view.SetContent(std::move(text));
            view.Layout({0, 0, 720, 720});
            RecordingCanvas canvas;
            view.Paint(canvas, {0, 0, 720, 720});
            assert(view.ContentSize().height > 200);
            assert(!canvas.drawnTexts.empty());
        }
    }

    return 0;
}
