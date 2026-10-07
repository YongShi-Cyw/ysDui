/**
 * 文件名：render_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证平台无关渲染、图像、显示列表与检查器逻辑。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;    const auto svg = ysDui::render::RasterizeSvg(
        "<svg width=\"4\" height=\"2\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect width=\"4\" height=\"2\" fill=\"#ff0000\"/></svg>");
    assert((svg && svg->Size() == Size{4, 2} && !svg->Empty()));
    assert(svg->Bytes()[0] == 0 && svg->Bytes()[1] == 0 && svg->Bytes()[2] == 255 && svg->Bytes()[3] == 255);
    const auto scaledSvg = ysDui::render::RasterizeSvg(
        "<svg width=\"4\" height=\"2\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect width=\"4\" height=\"2\" fill=\"#ff0000\"/></svg>", {8, 0});
    assert((scaledSvg && scaledSvg->Size() == Size{8, 4}));

    const auto clampedInsets = ysDui::render::ClampNinePatchInsets({10, 8}, {8, -1, 8, 9});
    assert((clampedInsets == ysDui::render::DuiNinePatchInsets{5, 0, 5, 8}));
    const auto ninePatchCells = ysDui::render::ComputeNinePatchCells({20, 20}, {0, 0, 10, 12}, {8, 6, 8, 6});
    assert((ninePatchCells[0] == ysDui::render::DuiNinePatchCell{{0, 0, 8, 6}, {0, 0, 5, 6}}));
    assert((ninePatchCells[4] == ysDui::render::DuiNinePatchCell{{8, 6, 12, 14}, {5, 6, 5, 6}}));
    RecordingCanvas focusCanvas;
    ysDui::render::DrawFocusRing(focusCanvas, {0, 0, 20, 20}, {45, 108, 223, 255});
    assert(focusCanvas.roundedStrokes == 2);
    focusCanvas.FillLinearGradient({1, 2, 31, 22}, 4, {{1, 2, 3, 255}, {4, 5, 6, 255}, false});
    assert((focusCanvas.linearGradientBounds == Rect{1, 2, 31, 22}));
    assert(focusCanvas.linearGradientRadius == 4);
    assert((focusCanvas.linearGradient.start == Color{1, 2, 3, 255}));
    assert((focusCanvas.linearGradient.end == Color{4, 5, 6, 255}));
    assert(!focusCanvas.linearGradient.vertical);
    focusCanvas.FillRadialGradient({2, 3, 32, 23}, 5, {{7, 8, 9, 255}, {10, 11, 12, 255}});
    assert((focusCanvas.radialGradientBounds == Rect{2, 3, 32, 23}));
    assert(focusCanvas.radialGradientRadius == 5);
    assert((focusCanvas.radialGradient.center == Color{7, 8, 9, 255}));
    assert((focusCanvas.radialGradient.edge == Color{10, 11, 12, 255}));
    auto golden = ysDui::render::DuiPixelBuffer::Create({2, 1}, {0, 0, 0, 255, 10, 10, 10, 255});
    auto actual = ysDui::render::DuiPixelBuffer::Create({2, 1}, {0, 0, 0, 255, 20, 10, 10, 255});
    const auto goldenDiff = ysDui::render::CompareGolden(actual, golden, 2);
    assert(goldenDiff.differingPixels == 1 && goldenDiff.maximumChannelDelta == 10);
    const auto diffImage = ysDui::render::CreateGoldenDiff(actual, golden, 2);
    assert(!diffImage.Empty() && diffImage.Bytes()[6] == 255);
    const std::filesystem::path svgPath = std::filesystem::temp_directory_path()
        / std::filesystem::path(u8"ysdui-资源-测试.svg");
    const std::u8string svgPathUtf8 = svgPath.u8string();
    const std::string svgPathText(reinterpret_cast<const char*>(svgPathUtf8.data()), svgPathUtf8.size());
    {
        std::ofstream svgFile(svgPath, std::ios::binary);
        svgFile << "<svg width=\"4\" height=\"3\" xmlns=\"http://www.w3.org/2000/svg\"/>";
    }
    const auto svgPixels = ysDui::render::RasterizeSvgFile(svgPathText);
    assert((svgPixels && svgPixels->Size() == Size{4, 3}));
    std::filesystem::remove(svgPath);

    FontRegistryMock fontRegistry;
    fontRegistry.families = {"Font Awesome 6 Free Solid"};
    ysDui::render::DuiIconFont iconFont(fontRegistry);
    assert(!iconFont.LoadFromFile({}, "Font Awesome 6 Free Solid"));
    assert(iconFont.LoadFromFile("icons.ttf", "Font Awesome 6 Free Solid"));
    assert(fontRegistry.file == "icons.ttf");
    assert(iconFont.LoadFromMemory({1, 2, 3}));
    assert(fontRegistry.memory.size() == 3 && iconFont.IsReady());
    assert(iconFont.Family() == "Font Awesome 6 Free Solid");
    const auto iconStyle = iconFont.TextStyle(14, {12, 34, 56, 255});
    assert(iconStyle.family == iconFont.Family() && iconStyle.pointSize == 14 && iconStyle.bold);
    RecordingCanvas iconCanvas;
    assert((iconFont.Measure(iconCanvas, "\uF013", iconStyle).size == Size{8, 10}));
    iconFont.Draw(iconCanvas, {1, 2, 21, 22}, "\uF013", iconStyle);
    assert(iconCanvas.drawnText == "\uF013" && iconCanvas.textStyle.family == iconFont.Family());
    assert(iconCanvas.textAlignment == ysDui::render::DuiTextAlignment::Center && !iconCanvas.wrapped);
    assert(!iconFont.SetFamily("Missing") && iconFont.IsReady());

    auto inspectorRoot = std::make_unique<ysDui::core::Control>();
    inspectorRoot->SetName("root");
    inspectorRoot->SetBounds({0, 0, 100, 80});
    auto inspectorChild = std::make_unique<ysDui::core::Control>();
    inspectorChild->SetName("child");
    inspectorChild->SetBounds({10, 12, 50, 40});
    ysDui::core::Control* inspectorChildRaw = inspectorChild.get();
    inspectorRoot->AddChild(std::move(inspectorChild));
    std::vector<Rect> inspectorBounds;
    ysDui::render::DuiInspector::CollectVisibleRects(*inspectorRoot, inspectorBounds);
    assert(inspectorBounds.size() == 2 && inspectorBounds[1] == inspectorChildRaw->Bounds());
    assert(ysDui::render::DuiInspector::FormatControlInfo(*inspectorChildRaw)
           == "Control name=child rect=(10,12,50,40)");
    inspectorChildRaw->SetName("\xE4\xB8\xAD\xE6\x96\x87");
    assert(ysDui::render::DuiInspector::FormatControlInfo(*inspectorChildRaw)
           == "Control name=\u4E2D\u6587 rect=(10,12,50,40)");
    inspectorChildRaw->SetBounds({(std::numeric_limits<int>::min)(), -1, 0,
                                  (std::numeric_limits<int>::max)()});
    assert(ysDui::render::DuiInspector::FormatControlInfo(*inspectorChildRaw)
           == "Control name=\u4E2D\u6587 rect=(-2147483648,-1,0,2147483647)");
    inspectorChildRaw->SetName("child");
    inspectorChildRaw->SetBounds({10, 12, 50, 40});
    ysDui::render::DuiInspector inspector;
    RecordingCanvas inspectorCanvas;
    inspector.PaintOverlay(*inspectorRoot, inspectorCanvas, {0, 0, 100, 80});
    assert(inspectorCanvas.roundedStrokes == 0);
    inspector.SetEnabled(true);
    inspectorChildRaw->SetHovered(true);
    inspector.PaintOverlay(*inspectorRoot, inspectorCanvas, {0, 0, 100, 80});
    assert(inspectorCanvas.roundedStrokes == 3 && inspectorCanvas.fills.size() == 1);
    assert(inspectorCanvas.drawnText == "Control name=child rect=(10,12,50,40)");
    ysDui::render::DisplayList list;
    list.FillRect({1, 2, 3, 4}, {1, 2, 3, 4});
    RecordingCanvas canvas;
    list.Replay(canvas);
    assert(canvas.fills.size() == 1);
    assert((canvas.fills.front().bounds == Rect{1, 2, 3, 4}));
    ysDui::render::DuiPath path;
    path.MoveTo({0, 0});
    path.LineTo({10, 0});
    path.LineTo({10, 10});
    path.Close();
    canvas.StrokePath(path, {0, 0, 0, 255}, 1.0f);
    assert(canvas.paths == 1);

    {
        RecordingCanvas transformed;
        transformed.PushTransform({2.0, {10, 20}});
        transformed.FillRect({1, 2, 4, 6}, {1, 2, 3, 255});
        transformed.PopTransform();
        assert(transformed.fills.size() == 1);
        assert((transformed.fills.front().bounds == Rect{12, 24, 18, 32}));
        transformed.FillRect({0, 0, 1, 1}, {0, 0, 0, 255});
        assert((transformed.fills.back().bounds == Rect{0, 0, 1, 1}));
    }

    ysDui::controls::basic::DuiSeparator separator;
    separator.SetBounds({0, 0, 20, 10});
    separator.SetInset(2);
    separator.SetThickness(2);
    RecordingCanvas separatorCanvas;
    separator.Paint(separatorCanvas, {0, 0, 20, 10});
    assert(separatorCanvas.fills.size() == 1);
    assert((separatorCanvas.fills.front().bounds == Rect{2, 4, 18, 6}));
    separator.SetOrientation(ysDui::controls::basic::DuiSeparator::Orientation::Vertical);
    separator.SetBounds({0, 0, 10, 20});
    separator.SetInset(3);
    separatorCanvas = {};
    separator.Paint(separatorCanvas, {0, 0, 10, 20});
    assert((separatorCanvas.fills.front().bounds == Rect{4, 3, 6, 17}));

    // 带文本的分割线：文本居中时两侧各留一段，文本与线条在竖直方向对齐。
    // RecordingCanvas 按 8px/字符估算宽度："TDesign" 7 字符 → 56px
    ysDui::controls::basic::DuiSeparator titled;
    titled.SetBounds({0, 0, 200, 20});
    titled.SetThickness(2);
    titled.SetText("TDesign");
    RecordingCanvas titledCanvas;
    titled.Paint(titledCanvas, {0, 0, 200, 20});
    assert(titledCanvas.fills.size() == 2);
    assert((titledCanvas.fills.front().bounds == Rect{0, 9, 64, 11}));
    assert((titledCanvas.fills.back().bounds == Rect{136, 9, 200, 11}));
    assert(titledCanvas.drawnText == "TDesign");
    assert((titledCanvas.textBounds == Rect{72, 0, 128, 20}));
    // 未显式设置文字样式时取主题的次要文本色
    assert((titledCanvas.textStyle.color == ysDui::core::DuiTheme{}.Get(ThemeSlot::TextSubtle)));

    // 对齐：靠左只保留右侧线段，靠右只保留左侧线段
    titled.SetTextAlign(ysDui::controls::basic::DuiSeparatorTextAlign::Left);
    titledCanvas = {};
    titled.Paint(titledCanvas, {0, 0, 200, 20});
    assert(titledCanvas.fills.size() == 1 && (titledCanvas.fills.front().bounds == Rect{64, 9, 200, 11}));
    assert((titledCanvas.textBounds == Rect{0, 0, 56, 20}));
    titled.SetTextAlign(ysDui::controls::basic::DuiSeparatorTextAlign::Right);
    titledCanvas = {};
    titled.Paint(titledCanvas, {0, 0, 200, 20});
    assert(titledCanvas.fills.size() == 1 && (titledCanvas.fills.front().bounds == Rect{0, 9, 136, 11}));
    assert((titledCanvas.textBounds == Rect{144, 0, 200, 20}));

    // 间距：文本两侧的空档随 TextGap 变化
    titled.SetTextAlign(ysDui::controls::basic::DuiSeparatorTextAlign::Center);
    titled.SetTextGap(20);
    titledCanvas = {};
    titled.Paint(titledCanvas, {0, 0, 200, 20});
    assert((titledCanvas.fills.front().bounds == Rect{0, 9, 52, 11}));
    assert((titledCanvas.fills.back().bounds == Rect{148, 9, 200, 11}));

    // 虚线：按 4px 段长 / 3px 间隔拆分
    ysDui::controls::basic::DuiSeparator dashed;
    dashed.SetBounds({0, 0, 30, 10});
    dashed.SetDashed(true);
    RecordingCanvas dashedCanvas;
    dashed.Paint(dashedCanvas, {0, 0, 30, 10});
    assert(dashedCanvas.fills.size() == 5);
    assert((dashedCanvas.fills.front().bounds == Rect{0, 5, 4, 6}));
    assert((dashedCanvas.fills.back().bounds == Rect{28, 5, 30, 6}));

    // 文本放不下时只画文本，不产生负宽度线段
    ysDui::controls::basic::DuiSeparator narrow;
    narrow.SetBounds({0, 0, 20, 20});
    narrow.SetText("TDesign");
    RecordingCanvas narrowCanvas;
    narrow.Paint(narrowCanvas, {0, 0, 20, 20});
    assert(narrowCanvas.fills.empty() && narrowCanvas.drawnText == "TDesign");

    // 纵向分割线不支持文本（需要旋转文字），仍只画线段
    ysDui::controls::basic::DuiSeparator verticalTitled;
    verticalTitled.SetOrientation(ysDui::controls::basic::DuiSeparator::Orientation::Vertical);
    verticalTitled.SetBounds({0, 0, 10, 40});
    verticalTitled.SetText("TDesign");
    verticalTitled.SetDashed(true);
    RecordingCanvas verticalCanvas;
    verticalTitled.Paint(verticalCanvas, {0, 0, 10, 40});
    assert(verticalCanvas.fills.size() == 1 && verticalCanvas.drawnTexts.empty());

}
