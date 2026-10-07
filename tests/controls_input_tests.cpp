/**
 * 文件名：controls_input_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证输入控件、富文本、选择器和属性编辑行为。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    ysDui::controls::input::DuiSpinBox spinBox;
    spinBox.SetRange(-2, 2);
    spinBox.SetStep(2);
    spinBox.SetWrap(true);
    spinBox.SetValue(3);
    assert(spinBox.Value() == -2);
    int parsed{};
    assert(ysDui::controls::input::DuiSpinBox::TryParseInt("-12", parsed) && parsed == -12);
    assert(!ysDui::controls::input::DuiSpinBox::TryParseInt("1a", parsed));
    spinBox.SetBounds({0, 0, 100, 24});
    bool spinChanged{};
    spinBox.SetValueChangedHandler([&spinChanged](int) { spinChanged = true; });
    assert(spinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {90, 5})));
    assert(spinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {90, 5})));
    assert(spinBox.Value() == 0 && spinChanged);
    ysDui::controls::input::DuiSpinBox rangeSpinBox;
    rangeSpinBox.SetWrap(true);
    rangeSpinBox.SetValue(2);
    rangeSpinBox.SetRange(0, 1);
    assert(rangeSpinBox.Value() == 1 && (rangeSpinBox.DesiredSize() == Size{120, 23}));
    rangeSpinBox.Layout({0, 0, 120, 23});
    assert(rangeSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {110, 5})));
    RecordingCanvas pressedSpinCanvas;
    rangeSpinBox.Paint(pressedSpinCanvas, rangeSpinBox.Bounds());
    assert(!pressedSpinCanvas.pathFillColors.empty()
        && (pressedSpinCanvas.pathFillColors.front() == Color{30, 30, 30, 255}));
    assert(rangeSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {110, 5})));
    RecordingCanvas idleSpinCanvas;
    rangeSpinBox.Paint(idleSpinCanvas, rangeSpinBox.Bounds());
    assert(idleSpinCanvas.fills.size() == 1 && idleSpinCanvas.filledPaths.size() == 2);
    for (const auto& arrow : idleSpinCanvas.filledPaths)
    {
        const auto& commands = arrow.Commands();
        assert(commands.size() == 4);
        int minimumX = commands[0].point.x;
        int maximumX = commands[0].point.x;
        int minimumY = commands[0].point.y;
        int maximumY = commands[0].point.y;
        for (const auto& pathCommand : commands)
        {
            if (pathCommand.type == ysDui::render::DuiPathCommandType::Close)
                continue;
            minimumX = (std::min)(minimumX, pathCommand.point.x);
            maximumX = (std::max)(maximumX, pathCommand.point.x);
            minimumY = (std::min)(minimumY, pathCommand.point.y);
            maximumY = (std::max)(maximumY, pathCommand.point.y);
        }
        assert(maximumX - minimumX == (maximumY - minimumY) * 2);
    }
    // 默认高度下字号为 12；高度放大 4 倍时字号与按钮条同比放大，避免画布缩放失调
    assert(!idleSpinCanvas.drawnTextStyles.empty()
        && idleSpinCanvas.drawnTextStyles.front().pointSize == 12);
    ysDui::controls::input::DuiSpinBox scaledSpinBox;
    scaledSpinBox.SetValue(172);
    scaledSpinBox.Layout({0, 0, 480, 92});
    RecordingCanvas scaledSpinCanvas;
    scaledSpinBox.Paint(scaledSpinCanvas, scaledSpinBox.Bounds());
    assert(!scaledSpinCanvas.drawnTextStyles.empty()
        && scaledSpinCanvas.drawnTextStyles.front().pointSize == 48);
    // 按钮条宽 = 缩放后 strip(72) − 内缩(4)；略高于默认高度时仍保持默认条宽
    assert(scaledSpinBox.UpRect().Width() == 68);
    assert(rangeSpinBox.UpRect().Width() == 17); // strip 18 − inset 1
    assert(scaledSpinBox.UpRect().Width() > rangeSpinBox.UpRect().Width());

    ysDui::controls::input::DuiEditHost editHost;
    assert(editHost.PointerCursor() == DuiPointerCursor::IBeam);
    TextInputMock editInput;
    ysDui::ui::DuiTextInputOptions editOptions;
    editOptions.multiline = true;
    editOptions.wordWrap = true;
    editOptions.maxLength = 32;
    editHost.SetText("Initial");
    editHost.SetPlaceholder("Type here");
    editHost.SetOptions(editOptions);
    editHost.Layout({2, 3, 122, 63});
    std::string editedText;
    editHost.SetTextChangedHandler([&editedText](std::string_view value) { editedText = value; });
    editHost.SetTextInput(&editInput);
    assert(editInput.text == "Initial" && editInput.placeholder == "Type here");
    assert(editInput.options.multiline && editInput.options.wordWrap && editInput.options.maxLength == 32);
    assert((editInput.bounds == Rect{7, 6, 117, 60}) && !editInput.borderVisible);
    editInput.SetText("Changed");
    assert(editHost.Text() == "Changed" && editedText == "Changed");
    assert(editHost.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {4, 4})) && editInput.focused);
    assert(editHost.Captured() && editInput.selectionBeginCount == 1
        && (editInput.selectionBegin == Point{4, 4}));
    assert(editHost.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {80, 20})));
    assert(editInput.selectionUpdateCount == 1 && (editInput.selectionUpdate == Point{80, 20}));
    assert(editHost.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {80, 20})));
    assert(!editHost.Captured() && editInput.selectionEndCount == 1);
    // 双击选词：指针层应把双击转发为平台选词请求
    assert(editHost.OnEvent(ysDui::test::MakeEvent(EventType::PointerDoubleClick, {30, 20})));
    assert(editInput.selectionWordCount == 1 && (editInput.selectionWord == Point{30, 20}));
    editHost.SetFocused(true);
    editOptions.multiline = false;
    editOptions.wordWrap = false;
    editHost.SetOptions(editOptions);
    editInput.selection = {1, 4};
    RecordingCanvas editCanvas;
    editHost.Paint(editCanvas, editHost.Bounds());
    assert(std::find(editCanvas.drawnTexts.begin(), editCanvas.drawnTexts.end(), "Changed")
           != editCanvas.drawnTexts.end());
    assert(std::find(editCanvas.drawnTexts.begin(), editCanvas.drawnTexts.end(), "han")
           != editCanvas.drawnTexts.end());
    assert(std::any_of(editCanvas.fills.begin(), editCanvas.fills.end(), [&editHost](const auto& fill)
    {
        return fill.color == editHost.Theme().Get(ThemeSlot::TextSelectionBackground)
            && fill.bounds.Width() == 24;
    }));
    RecordingCanvas partialEditCanvas;
    editHost.Paint(partialEditCanvas, editInput.Bounds());
    assert(partialEditCanvas.strokeBounds == editHost.Bounds());
    editHost.SetTextInput(nullptr);
    assert(!editInput.changed && !editInput.focusLost);

    // Enter 提交契约（Canvas 回退路径：未绑定原生输入，由控件自身判定）
    {
        ysDui::controls::input::DuiEditHost submitHost;
        int submits{};
        submitHost.SetSubmitHandler([&submits] { ++submits; });
        submitHost.Layout({0, 0, 200, 60});
        ysDui::ui::DuiTextInputOptions submitOptions;
        submitOptions.multiline = false;
        submitHost.SetOptions(submitOptions);
        // 单行：Enter 提交，Shift+Enter 也提交（修饰键不改变单行行为）
        assert(submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 1);
        assert(submitHost.OnEvent(ysDui::test::MakeEvent(
            EventType::KeyDown, {}, ysDui::core::key::Enter, ysDui::core::modifier::Shift)));
        assert(submits == 2);
        // 其它按键不触发
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Right)));
        assert(submits == 2);
        // 多行且未开启 submitOnEnter：Enter 留给换行，不提交
        submitOptions.multiline = true;
        submitHost.SetOptions(submitOptions);
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 2);
        // 多行 + submitOnEnter：Enter 提交，Shift+Enter 仍为换行
        submitOptions.submitOnEnter = true;
        submitHost.SetOptions(submitOptions);
        assert(submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 3);
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(
            EventType::KeyDown, {}, ysDui::core::key::Enter, ysDui::core::modifier::Shift)));
        assert(submits == 3);
        // 只读输入不接受提交
        submitOptions.readOnly = true;
        submitHost.SetOptions(submitOptions);
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 3);
        // 禁用后不接受提交
        submitOptions.readOnly = false;
        submitHost.SetOptions(submitOptions);
        submitHost.SetEnabled(false);
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 3);
        // 绑定原生输入后，提交由平台回调驱动，控件不再自行判定
        submitHost.SetEnabled(true);
        TextInputMock submitInput;
        submitHost.SetTextInput(&submitInput);
        assert(!submitHost.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Enter)));
        assert(submits == 3);
        submitInput.Submit();
        assert(submits == 4);
        // 解绑后平台回调不再触达本控件
        submitHost.SetTextInput(nullptr);
        submitInput.Submit();
        assert(submits == 4);
    }

    ysDui::controls::input::DuiRichEditHost richEditHost;
    RichTextInputMock richInput;
    richEditHost.SetText("Hello");
    richEditHost.SetSelection({1, 4});
    richEditHost.SetAutomaticLinkDetection(true);
    richEditHost.Layout({3, 4, 203, 104});
    std::string richLink;
    richEditHost.SetLinkActivatedHandler([&richLink](std::string_view value) { richLink = value; });
    richEditHost.SetRichTextInput(&richInput);
    assert((richInput.text == "Hello" && richInput.selection == ysDui::ui::DuiTextRange{1, 4}));
    assert(richInput.automaticLinkDetection && (richInput.bounds == Rect{3, 4, 203, 104}));
    richEditHost.ReplaceSelection("i");
    assert((richEditHost.Text() == "Hio" && richEditHost.Selection() == ysDui::ui::DuiTextRange{2, 2}));
    richEditHost.SetSelectionFormat({{20, 30, 40, 255}, true, true, false});
    assert(richInput.format.bold && richInput.format.italic && richInput.format.color.red == 20);
    richInput.EmitLink("https://example.test");
    assert(richLink == "https://example.test" && richEditHost.CanUndo());
    richEditHost.Undo();
    assert(richInput.undone);
    richEditHost.InsertQuoteBlock("Alice", "Hello");
    richEditHost.InsertFileCard("report.txt", 42);
    assert(richInput.quoteSender == "Alice" && richInput.quoteBody == "Hello");
    assert(richInput.fileName == "report.txt" && richInput.fileSize == 42);
    richEditHost.SetRichTextInput(nullptr);
    assert(!richInput.changed && !richInput.focusLost && !richInput.linkActivated);

    ysDui::controls::input::DuiRichDocument richDocument;
    richDocument.AddTextBlock({{"Hello", {{10, 20, 30, 255}, true, false, false}, "https://example.test"}});
    richDocument.AddQuoteBlock("Alice", "Quoted text");
    richDocument.AddFileCard("report.txt", 2048);
    ysDui::controls::input::DuiRichDocument richDocumentCopy = richDocument;
    assert(richDocumentCopy.Blocks() == richDocument.Blocks() && richDocumentCopy.Blocks().size() == 3);
    richDocumentCopy.AddImageBlock("images/logo.png", {32, 16});
    const std::string richDocumentJson = ysDui::controls::input::DuiRichDocumentJson::Serialize(richDocumentCopy);
    const auto parsedRichDocument = ysDui::controls::input::DuiRichDocumentJson::Deserialize(richDocumentJson);
    assert(parsedRichDocument && parsedRichDocument->Blocks() == richDocumentCopy.Blocks());
    assert(richDocumentJson.find("\"resource\":\"images/logo.png\"") != std::string::npos);
    const std::size_t richDocumentBlockCount = richDocumentCopy.Blocks().size();
    richDocumentCopy.AddImageBlock("", {-1, -1});
    assert(richDocumentCopy.Blocks().size() == richDocumentBlockCount);
    const auto escapedRichDocument = ysDui::controls::input::DuiRichDocumentJson::Deserialize(
        "{\"version\":1,\"blocks\":[{\"kind\":\"text\",\"runs\":[{\"text\":\"\\u4F60\\u597D\","
        "\"color\":[1,2,3,4],\"bold\":false,\"italic\":false,\"underline\":false,\"link\":\"\"}]}]}");
    assert(escapedRichDocument && escapedRichDocument->Blocks()[0].runs[0].text == "\u4F60\u597D");
    assert(!ysDui::controls::input::DuiRichDocumentJson::Deserialize("{\"version\":2,\"blocks\":[]}"));
    assert(!ysDui::controls::input::DuiRichDocumentJson::Deserialize(
        "{\"version\":1,\"blocks\":[{\"kind\":\"image\",\"resource\":\"\",\"width\":1,\"height\":1}]}"));
    richDocument.Clear();
    assert(richDocument.Empty() && !richDocumentCopy.Empty());

    std::atomic<int> imageDecodeCount{};
    ysDui::controls::media::DuiAsyncImageLoader imageLoader;
    imageLoader.SetDecoder([&imageDecodeCount](const std::string&) {
        ++imageDecodeCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return ysDui::render::DuiImage::CreateBgra8Premultiplied({1, 1}, {255, 0, 0, 255});
    });
    const std::uint64_t firstImageRequest = imageLoader.Submit("avatar.png", 11);
    const std::uint64_t secondImageRequest = imageLoader.Submit("avatar.png", 12);
    std::vector<ysDui::controls::media::DuiAsyncImageResult> imageResults;
    for (int attempt = 0; attempt < 100 && imageResults.size() < 2; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::vector<ysDui::controls::media::DuiAsyncImageResult> pendingImageResults = imageLoader.Poll();
        imageResults.insert(imageResults.end(), std::make_move_iterator(pendingImageResults.begin()),
                            std::make_move_iterator(pendingImageResults.end()));
    }
    assert(imageDecodeCount == 1 && imageResults.size() == 2);
    assert(imageResults[0].error == ysDui::controls::media::DuiAsyncImageError::Succeeded);
    assert(imageResults[0].image && imageResults[1].image);
    assert(firstImageRequest != 0 && secondImageRequest != 0);
    const std::uint64_t cachedImageRequest = imageLoader.Submit("avatar.png", 13);
    const auto cachedImageResults = imageLoader.Poll();
    assert(cachedImageRequest != 0 && cachedImageResults.size() == 1 && imageDecodeCount == 1);
    const std::uint64_t cancelledImageRequest = imageLoader.Submit("cancel.png", 14);
    imageLoader.Cancel(cancelledImageRequest);
    const auto cancelledImageResults = imageLoader.Poll();
    assert(cancelledImageResults.size() == 1 && cancelledImageResults[0].requestId == cancelledImageRequest);
    assert(cancelledImageResults[0].error == ysDui::controls::media::DuiAsyncImageError::Cancelled);

    ysDui::controls::input::DuiSpinBox editableSpinBox;
    editableSpinBox.SetRange(-20, 20);
    editableSpinBox.Layout({0, 0, 100, 24});
    TextInputMock textInput;
    int committedValue{};
    editableSpinBox.SetValueChangedHandler([&committedValue](int value) { committedValue = value; });
    textInput.borderVisible = true;
    editableSpinBox.SetTextInput(&textInput);
    // 文本矩形垂直居中于控件（上/下对称内缩），避免数字视觉偏上
    assert(!textInput.borderVisible && (textInput.Bounds() == Rect{3, 3, 82, 21}));
    assert(textInput.Text() == "0");
    textInput.SetText("12x");
    assert(textInput.Text() == "12");
    RecordingCanvas proxySpinCanvas;
    editableSpinBox.Paint(proxySpinCanvas, editableSpinBox.Bounds());
    assert(std::find(proxySpinCanvas.drawnTexts.begin(), proxySpinCanvas.drawnTexts.end(), "12")
           != proxySpinCanvas.drawnTexts.end());
    assert(std::find(proxySpinCanvas.drawnTextBounds.begin(), proxySpinCanvas.drawnTextBounds.end(),
                     Rect{3, 3, 82, 21}) != proxySpinCanvas.drawnTextBounds.end());
    RecordingCanvas partialSpinCanvas;
    editableSpinBox.Paint(partialSpinCanvas, textInput.Bounds());
    assert(partialSpinCanvas.strokeBounds == editableSpinBox.Bounds());
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10})));
    assert(editableSpinBox.PointerCursor() == DuiPointerCursor::IBeam);
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {30, 10})));
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {30, 10})));
    assert(textInput.selectionBeginCount == 1 && textInput.selectionUpdateCount == 1
        && textInput.selectionEndCount == 1 && !editableSpinBox.Captured());
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerLeave, {130, 10})));
    assert(!editableSpinBox.Hovered() && editableSpinBox.PointerCursor() == DuiPointerCursor::Arrow);
    textInput.LoseFocus();
    assert(editableSpinBox.Value() == 12 && committedValue == 12);
    textInput.SetText("-");
    textInput.LoseFocus();
    assert(textInput.Text() == "12");
    textInput.SetText("9");
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {90, 5})));
    assert(editableSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {90, 5})));
    assert(editableSpinBox.Value() == 10 && textInput.Text() == "10");
    editableSpinBox.SetTextInput(nullptr);
    assert(!textInput.changed && !textInput.focusLost);
    editableSpinBox.SetFocused(true);
    RecordingCanvas focusedSpinCanvas;
    editableSpinBox.Paint(focusedSpinCanvas, editableSpinBox.Bounds());
    assert((focusedSpinCanvas.strokeColor == Color{80, 130, 200, 255}));
    editableSpinBox.SetFocused(false);
    editableSpinBox.SetEnabled(false);
    RecordingCanvas disabledSpinCanvas;
    editableSpinBox.Paint(disabledSpinCanvas, editableSpinBox.Bounds());
    assert((disabledSpinCanvas.strokeColor == Color{190, 190, 190, 255}));

    ysDui::controls::input::DuiDoubleSpinBox doubleSpinBox;
    doubleSpinBox.SetRange(-2.0, 2.0);
    doubleSpinBox.SetStep(0.25);
    doubleSpinBox.SetDecimals(2);
    doubleSpinBox.Layout({0, 0, 100, 24});
    TextInputMock doubleTextInput;
    double committedDouble{};
    doubleSpinBox.SetValueChangedHandler([&committedDouble](double value) { committedDouble = value; });
    doubleTextInput.borderVisible = true;
    doubleSpinBox.SetTextInput(&doubleTextInput);
    // 与 DuiSpinBox 一致：文本矩形垂直居中于控件
    assert(!doubleTextInput.borderVisible && doubleTextInput.Text() == "0.00"
        && (doubleTextInput.Bounds() == Rect{3, 3, 82, 21}));
    RecordingCanvas partialDoubleSpinCanvas;
    doubleSpinBox.Paint(partialDoubleSpinCanvas, doubleTextInput.Bounds());
    assert(partialDoubleSpinCanvas.strokeBounds == doubleSpinBox.Bounds());
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {10, 10})));
    assert(doubleSpinBox.PointerCursor() == DuiPointerCursor::IBeam);
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {30, 10})));
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {30, 10})));
    assert(doubleTextInput.selectionBeginCount == 1 && doubleTextInput.selectionUpdateCount == 1
        && doubleTextInput.selectionEndCount == 1 && !doubleSpinBox.Captured());
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerLeave, {130, 10})));
    assert(!doubleSpinBox.Hovered() && doubleSpinBox.PointerCursor() == DuiPointerCursor::Arrow);
    doubleTextInput.SetText("1.239x");
    assert(doubleTextInput.Text() == "1.23");
    doubleTextInput.LoseFocus();
    assert(doubleSpinBox.Value() == 1.23 && committedDouble == 1.23);
    doubleTextInput.SetText("-");
    doubleTextInput.LoseFocus();
    assert(doubleTextInput.Text() == "1.23");
    doubleTextInput.SetText("1.50");
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {90, 5})));
    assert(doubleSpinBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {90, 5})));
    assert(doubleSpinBox.Value() == 1.75 && doubleTextInput.Text() == "1.75");
    assert(ysDui::controls::input::DuiDoubleSpinBox::TryParseDouble("-0.25", committedDouble));
    assert(committedDouble == -0.25);
    doubleSpinBox.SetTextInput(nullptr);
    doubleSpinBox.SetFocused(true);
    RecordingCanvas focusedDoubleSpinCanvas;
    doubleSpinBox.Paint(focusedDoubleSpinCanvas, doubleSpinBox.Bounds());
    assert((focusedDoubleSpinCanvas.strokeColor == Color{80, 130, 200, 255}));
    ysDui::controls::input::DuiDoubleSpinBox rangeDoubleSpinBox;
    rangeDoubleSpinBox.SetWrap(true);
    rangeDoubleSpinBox.SetValue(2.0);
    rangeDoubleSpinBox.SetRange(0.0, 1.0);
    assert(rangeDoubleSpinBox.Value() == 1.0
        && (rangeDoubleSpinBox.DesiredSize() == Size{120, 23}));

    ysDui::controls::input::DuiSearchBox searchBox;
    assert(searchBox.PointerCursor() == DuiPointerCursor::Arrow);
    searchBox.SetPlaceholder("Search");
    searchBox.Layout({0, 0, 180, 28});
    TextInputMock searchInput;
    std::string searchText;
    searchBox.SetTextChangedHandler([&searchText](std::string_view text) { searchText = text; });
    searchBox.SetTextInput(&searchInput);
    assert(searchInput.placeholder == "Search" && !searchInput.borderVisible);
    assert(!searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {40, 14})));
    assert(searchBox.PointerCursor() == DuiPointerCursor::IBeam);
    searchInput.SetText("needle");
    assert(searchBox.ClearShowing() && searchText == "needle"
        && (searchInput.Bounds() == Rect{29, 3, searchBox.ClearRect().left - 5, 25}));
    assert(searchBox.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Escape)));
    assert(searchBox.Text().empty() && searchText.empty() && !searchBox.ClearShowing());
    searchInput.SetText("needle");
    assert(searchBox.ClearShowing() && searchText == "needle");
    assert(searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {40, 14})));
    assert(searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {80, 14})));
    assert(searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {80, 14})));
    assert(searchInput.selectionBeginCount == 1 && searchInput.selectionUpdateCount == 1
        && searchInput.selectionEndCount == 1 && !searchBox.Captured());
    assert(!searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {170, 14})));
    assert(searchBox.PointerCursor() == DuiPointerCursor::Hand);
    searchBox.SetFocused(true);
    RecordingCanvas proxySearchCanvas;
    searchBox.Paint(proxySearchCanvas, searchBox.Bounds());
    assert(std::find(proxySearchCanvas.drawnTexts.begin(), proxySearchCanvas.drawnTexts.end(), "needle")
           != proxySearchCanvas.drawnTexts.end());
    assert(std::any_of(proxySearchCanvas.fills.begin(), proxySearchCanvas.fills.end(),
                       [](const auto& fill)
                       {
                           return fill.bounds.Width() == 1 && fill.bounds.Height() == 10;
                       }));
    searchInput.caretVisible = false;
    RecordingCanvas hiddenCaretSearchCanvas;
    searchBox.Paint(hiddenCaretSearchCanvas, searchBox.Bounds());
    assert(std::none_of(hiddenCaretSearchCanvas.fills.begin(), hiddenCaretSearchCanvas.fills.end(),
                        [](const auto& fill) { return fill.bounds.Width() == 1; }));
    searchInput.caretVisible = true;
    searchBox.SetFocused(false);
    assert(searchBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {170, 14})));
    assert(searchBox.Text().empty() && searchText.empty());
    searchBox.SetTextInput(nullptr);
    RecordingCanvas searchBoxCanvas;
    searchBox.Paint(searchBoxCanvas, {0, 0, 180, 28});
    assert(std::find(searchBoxCanvas.drawnTexts.begin(), searchBoxCanvas.drawnTexts.end(), "Search")
           != searchBoxCanvas.drawnTexts.end());
    searchBox.SetText("needle");
    searchBoxCanvas.drawnTexts.clear();
    searchBox.Paint(searchBoxCanvas, {0, 0, 180, 28});
    assert(std::find(searchBoxCanvas.drawnTexts.begin(), searchBoxCanvas.drawnTexts.end(), "needle")
           != searchBoxCanvas.drawnTexts.end());
    RecordingCanvas searchBoxClearCanvas;
    searchBox.Paint(searchBoxClearCanvas, searchBox.ClearRect());
    assert((searchBoxClearCanvas.arcBounds == Rect{6, 7, 16, 17}));
    searchBox.SetFocused(true);
    RecordingCanvas focusedSearchCanvas;
    searchBox.Paint(focusedSearchCanvas, searchBox.Bounds());
    assert((focusedSearchCanvas.strokeColor == Color{80, 130, 200, 255}));
    searchBox.SetFocused(false);
    searchBox.SetEnabled(false);
    RecordingCanvas disabledSearchCanvas;
    searchBox.Paint(disabledSearchCanvas, searchBox.Bounds());
    assert((disabledSearchCanvas.strokeColor == Color{190, 190, 190, 255}));

    ysDui::controls::input::DuiPathEdit pathEdit;
    pathEdit.Layout({0, 0, 240, 25});
    TextInputMock pathInput;
    PathPickerMock pathPicker;
    pathEdit.SetTextInput(&pathInput);
    pathEdit.SetPathPicker(&pathPicker);
    pathEdit.SetPath("C:/initial");
    std::string chosenPath;
    pathEdit.SetPathChangedHandler([&chosenPath](std::string_view path) { chosenPath = path; });
    pathPicker.result = {true, "C:/chosen"};
    assert(pathEdit.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {220, 12})));
    assert(pathPicker.mode == ysDui::ui::DuiPathPickerMode::OpenFile);
    assert(pathPicker.initialPath == "C:/initial" && pathEdit.Path() == "C:/chosen" && chosenPath == "C:/chosen");
    RecordingCanvas proxyPathCanvas;
    pathEdit.Paint(proxyPathCanvas, pathEdit.Bounds());
    assert(std::find(proxyPathCanvas.drawnTexts.begin(), proxyPathCanvas.drawnTexts.end(), "C:/chosen")
           != proxyPathCanvas.drawnTexts.end());
    pathEdit.SetBrowseMode(ysDui::ui::DuiPathPickerMode::Folder);
    pathPicker.result = {false, {}};
    assert(pathEdit.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {220, 12})));
    assert(pathPicker.mode == ysDui::ui::DuiPathPickerMode::Folder && pathEdit.Path() == "C:/chosen");
    pathEdit.SetBrowseMode(ysDui::ui::DuiPathPickerMode::SaveFile);
    pathPicker.result = {true, "C:/saved.txt"};
    assert(pathEdit.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {220, 12})));
    assert(pathPicker.mode == ysDui::ui::DuiPathPickerMode::SaveFile && pathEdit.Path() == "C:/saved.txt");

    ysDui::controls::input::DuiComboBox comboBox;
    assert(comboBox.PointerCursor() == DuiPointerCursor::Hand);
    comboBox.Layout({0, 0, 120, 25});
    comboBox.AddItem("Apple");
    comboBox.AddItem("Banana");
    comboBox.AddItem("Apricot");
    int selectedCombo = -2;
    comboBox.SetSelectionChangedHandler([&selectedCombo](int index) { selectedCombo = index; });
    comboBox.SetSelectedIndex(1);
    assert(comboBox.Text() == "Banana" && selectedCombo == 1);
    assert(comboBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerWheel, {10, 10}, 0, 0, 0, -120)));
    assert(comboBox.SelectedIndex() == 2);
    comboBox.SetSubstringSearch(false);
    assert((comboBox.FilteredIndices("Ap") == std::vector<int>{0, 2}));
    comboBox.SetArrowColor({45, 108, 223, 255});
    assert((comboBox.ArrowColor() == Color{45, 108, 223, 255}));
    PopupHostMock comboPopup;
    comboBox.SetPopupHost(&comboPopup);
    assert(comboBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})) && comboBox.PopupOpen());
    assert((comboPopup.options.size == Size{120, 66}) && comboPopup.content != nullptr);
    comboPopup.requestedHide = false;
    assert(comboBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {110, 10})));
    assert(comboPopup.requestedHide && !comboBox.PopupOpen());
    comboPopup.requestedHide = false;
    assert(comboBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {110, 10}))
        && comboBox.PopupOpen());
    RecordingCanvas comboPopupCanvas;
    comboPopup.paint(comboPopupCanvas, {0, 0, 120, 66});
    assert(comboPopupCanvas.fills.size() >= 4);
    const auto comboBorder = comboPopupCanvas.fills.end() - 4;
    assert((comboBorder[0].bounds == Rect{0, 0, 120, 1}
        && comboBorder[1].bounds == Rect{0, 65, 120, 66}
        && comboBorder[2].bounds == Rect{0, 1, 1, 65}
        && comboBorder[3].bounds == Rect{119, 1, 120, 65}));

    ysDui::controls::input::DuiAutoSuggestBox suggest;
    suggest.SetSuggestions({"Apple", "Apricot", "Banana"});
    assert(suggest.SuggestionCount() == 3);
    suggest.SetSuggestionProvider([](std::string_view query)
    {
        if (query.empty())
            return std::vector<std::string>{};
        return std::vector<std::string>{"Alpha", "Alpine"};
    });
    suggest.SetText("Al");
    assert(suggest.SuggestionCount() == 2 && suggest.Text() == "Al");
    assert(std::all_of(comboBorder, comboPopupCanvas.fills.end(), [](const auto& fill)
    {
        return fill.color == Color{170, 170, 170, 255};
    }));
    assert(comboPopup.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(comboBox.SelectedIndex() == 0 && comboPopup.requestedHide && !comboBox.PopupOpen());
    const auto comboIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 2}, std::vector<unsigned char>(16, 255));
    const auto comboItemIcon = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 2}, std::vector<unsigned char>(16, 128));
    comboBox.SetLeadingIcon(comboIcon);
    comboBox.SetLeadingIconSize({12, 12});
    comboBox.SetLeadingIconGap(4);
    RecordingCanvas comboCanvas;
    comboBox.Paint(comboCanvas, comboBox.Bounds());
    assert(comboBox.LeadingIcon() == comboIcon && (comboBox.LeadingIconSize() == Size{12, 12})
        && comboBox.LeadingIconGap() == 4);
    assert(comboCanvas.images == 1 && (comboCanvas.imageDestination == Rect{8, 6, 20, 18})
        && comboCanvas.textBounds.left == 24);
    comboBox.SetItemIcon(0, comboItemIcon);
    assert(comboBox.ItemIconAt(0) == comboItemIcon && !comboBox.ItemIconAt(-1));
    comboCanvas = {};
    comboBox.Paint(comboCanvas, comboBox.Bounds());
    assert(comboCanvas.lastImage == comboItemIcon.get());
    assert(comboBox.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {110, 10}))
        && comboBox.PopupOpen());
    const auto* comboPopupList = dynamic_cast<const ysDui::controls::list::DuiListBox*>(
        comboPopup.content.get());
    assert(comboPopupList != nullptr && comboPopupList->LeadingIcon() == comboIcon
        && (comboPopupList->LeadingIconSize() == Size{12, 12})
        && comboPopupList->LeadingIconGap() == 4 && comboPopupList->IconAt(0) == comboItemIcon);
    RecordingCanvas comboIconPopupCanvas;
    comboPopup.paint(comboIconPopupCanvas, {0, 0, 120, 66});
    assert(comboIconPopupCanvas.images == 3
        && (comboIconPopupCanvas.imageDestination == Rect{6, 49, 18, 61})
        && comboIconPopupCanvas.textBounds.left == 22);
    comboBox.ClosePopup();
    comboBox.SetIncrementalSearch(true);
    comboBox.SetItemHeight(28);
    comboBox.SetText("Apricot");
    comboBox.OpenPopup();
    const auto* filteredPopupList = dynamic_cast<const ysDui::controls::list::DuiListBox*>(comboPopup.content.get());
    assert(filteredPopupList != nullptr && filteredPopupList->Count() == 1 && filteredPopupList->SelectedIndex() == 0
        && filteredPopupList->RowHeight() == 28 && (comboPopup.options.size == Size{120, 28}));
    comboBox.ClosePopup();
    comboBox.SetEditable(true);
    comboCanvas = {};
    comboBox.Paint(comboCanvas, comboBox.Bounds());
    assert(comboCanvas.images == 0);
    TextInputMock comboInput;
    comboBox.SetTextInput(&comboInput);
    comboInput.SetText("Custom");
    assert(comboBox.Text() == "Custom" && comboBox.SelectedIndex() == -1);
    RecordingCanvas proxyComboCanvas;
    comboBox.Paint(proxyComboCanvas, comboBox.Bounds());
    assert(std::find(proxyComboCanvas.drawnTexts.begin(), proxyComboCanvas.drawnTexts.end(), "Custom")
           != proxyComboCanvas.drawnTexts.end());
    RecordingCanvas partialComboCanvas;
    comboBox.Paint(partialComboCanvas, comboInput.Bounds());
    assert(partialComboCanvas.strokeBounds == comboBox.Bounds());
    comboBox.SetTextInput(nullptr);
    comboBox.SetPopupHost(nullptr);

    ysDui::controls::input::DuiColorPicker colorPicker;
    colorPicker.Layout({0, 0, 140, 25});
    PopupHostMock colorPopup;
    ColorChooserMock colorChooser;
    ysDui::core::Color changedColor;
    colorPicker.SetPopupHost(&colorPopup);
    colorPicker.SetColorChooser(&colorChooser);
    colorPicker.SetColorChangedHandler([&changedColor](ysDui::core::Color color) { changedColor = color; });
    assert(ysDui::controls::input::DuiColorPicker::FormatColorHex({45, 108, 223}) == "#2D6CDF");
    assert((ysDui::controls::input::DuiColorPicker::ParseColorHex("#2D6CDF") == ysDui::core::Color{45, 108, 223, 255}));
    assert((ysDui::controls::input::DuiColorPicker::ParseColorHex("bad") == ysDui::core::Color{}));
    assert(colorPicker.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})) && colorPicker.PopupOpen());
    assert((colorPopup.options.size == ysDui::core::Size{156, 126}));
    RecordingCanvas colorPopupCanvas;
    colorPopup.paint(colorPopupCanvas, {0, 0, 156, 126});
    assert(std::find(colorPopupCanvas.drawnTexts.begin(), colorPopupCanvas.drawnTexts.end(),
                     "More\xE2\x80\xA6") != colorPopupCanvas.drawnTexts.end());
    assert(colorPopup.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {7, 7})));
    assert((colorPicker.Color() == ysDui::core::Color{0, 0, 0, 255} && changedColor == colorPicker.Color() && colorPopup.requestedHide));
    colorPopup.requestedHide = false;
    colorChooser.result = {true, {1, 2, 3, 255}};
    colorPicker.OpenPopup();
    assert(colorPopup.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {8, 102})));
    assert((colorChooser.initial == ysDui::core::Color{0, 0, 0, 255}));
    assert((colorPicker.Color() == ysDui::core::Color{1, 2, 3, 255} && colorPopup.requestedHide));
    colorPicker.SetFocused(true);
    RecordingCanvas focusedColorPickerCanvas;
    colorPicker.Paint(focusedColorPickerCanvas, colorPicker.Bounds());
    // 默认无焦点线框：只剩字段边框
    assert(focusedColorPickerCanvas.roundedStrokes == 1);
    DuiTheme focusRingPickerTheme;
    focusRingPickerTheme.SetFocusRingVisible(true);
    colorPicker.SetTheme(&focusRingPickerTheme);
    RecordingCanvas themedColorPickerCanvas;
    colorPicker.Paint(themedColorPickerCanvas, colorPicker.Bounds());
    assert(themedColorPickerCanvas.roundedStrokes == 3);

    ysDui::controls::input::DuiMonthCalendar monthCalendar;
    monthCalendar.Layout({0, 0, 240, 230});
    monthCalendar.SetViewMonth(2024, 2);
    assert(monthCalendar.DayAtCell(4) == 1);
    assert(monthCalendar.DayAtCell(32) == 29);
    assert((monthCalendar.CellFromPoint({18, 66}) == Point{0, 0}));
    DateTime chosenDate;
    monthCalendar.SetDateSelectedHandler([&chosenDate](DateTime value) { chosenDate = value; });
    assert(monthCalendar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {150, 66})));
    assert((chosenDate == DateTime{2024, 2, 1}));
    monthCalendar.SetMinDate(DateTime{2024, 2, 10});
    assert(monthCalendar.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {150, 66})));
    assert((monthCalendar.Selected() == DateTime{2024, 2, 10}));
    RecordingCanvas monthCalendarCanvas;
    monthCalendar.Paint(monthCalendarCanvas, {0, 0, 240, 230});
    assert(std::find(monthCalendarCanvas.drawnTexts.begin(), monthCalendarCanvas.drawnTexts.end(), "Su")
           != monthCalendarCanvas.drawnTexts.end());
    assert(std::find(monthCalendarCanvas.drawnTexts.begin(), monthCalendarCanvas.drawnTexts.end(), "Tu")
           != monthCalendarCanvas.drawnTexts.end());
    assert(std::find(monthCalendarCanvas.drawnTexts.begin(), monthCalendarCanvas.drawnTexts.end(), "10")
           != monthCalendarCanvas.drawnTexts.end());
    assert(std::find(monthCalendarCanvas.drawnTexts.begin(), monthCalendarCanvas.drawnTexts.end(), "29")
           != monthCalendarCanvas.drawnTexts.end());

    ysDui::controls::input::DuiDateTimePicker dateTimePicker;
    DateTimeSourceMock dateTimeSource;
    PopupHostMock dateTimePopup;
    dateTimePicker.Layout({5, 6, 165, 31});
    dateTimePicker.SetDateTimeSource(&dateTimeSource);
    dateTimePicker.SetPopupHost(&dateTimePopup);
    dateTimePicker.SetDate({2024, 2, 20, 13, 14, 15});
    DateTime parsedDate;
    assert(ysDui::controls::input::DuiDateTimePicker::TryParseYmd("2024-02-29", parsedDate));
    assert(!ysDui::controls::input::DuiDateTimePicker::TryParseYmd("2023-02-29", parsedDate));
    assert(ysDui::controls::input::DuiDateTimePicker::TryParseHms("13:14", parsedDate));
    assert(parsedDate.hour == 13 && parsedDate.minute == 14 && parsedDate.second == 0);
    assert(ysDui::controls::input::DuiDateTimePicker::TryParseYmdHms("2024-02-29 13:14:15", parsedDate));
    assert(ysDui::controls::input::DuiDateTimePicker::FormatDate(parsedDate, "%Y/%m/%d %H:%M:%S") == "2024/02/29 13:14:15");
    RecordingCanvas datePickerCanvas;
    dateTimePicker.SetMode(ysDui::controls::input::DuiDateTimePicker::Mode::Date);
    assert((dateTimePicker.DesiredSize() == Size{140, 25}));
    dateTimePicker.SetDate(DateTime{2024, 2, 20});
    dateTimePicker.Paint(datePickerCanvas, {5, 6, 165, 31});
    assert(std::find(datePickerCanvas.drawnTexts.begin(), datePickerCanvas.drawnTexts.end(), "2024-02-20")
           != datePickerCanvas.drawnTexts.end());
    datePickerCanvas.drawnTexts.clear();
    dateTimePicker.SetMode(ysDui::controls::input::DuiDateTimePicker::Mode::Time);
    assert((dateTimePicker.DesiredSize() == Size{120, 25}));
    dateTimePicker.SetDate(DateTime{2024, 2, 20, 13, 14, 15});
    dateTimePicker.Paint(datePickerCanvas, {5, 6, 165, 31});
    assert(std::find(datePickerCanvas.drawnTexts.begin(), datePickerCanvas.drawnTexts.end(), "13:14:15")
           != datePickerCanvas.drawnTexts.end());
    datePickerCanvas.drawnTexts.clear();
    dateTimePicker.SetMode(ysDui::controls::input::DuiDateTimePicker::Mode::DateTime);
    assert((dateTimePicker.DesiredSize() == Size{200, 25}));
    dateTimePicker.SetDate(DateTime{2024, 2, 20, 13, 14, 15});
    dateTimePicker.Paint(datePickerCanvas, {5, 6, 165, 31});
    assert(std::find(datePickerCanvas.drawnTexts.begin(), datePickerCanvas.drawnTexts.end(), "2024-02-20 13:14:15")
           != datePickerCanvas.drawnTexts.end());
    assert(!datePickerCanvas.pathFillColors.empty()
        && (datePickerCanvas.pathFillColors.back() == Color{80, 80, 90, 255}));
    dateTimePicker.SetFocused(true);
    RecordingCanvas focusedDateTimeCanvas;
    dateTimePicker.Paint(focusedDateTimeCanvas, dateTimePicker.Bounds());
    // 默认无焦点线框：只剩字段边框
    assert(focusedDateTimeCanvas.roundedStrokes == 1);
    DuiTheme focusRingDateTimeTheme;
    focusRingDateTimeTheme.SetFocusRingVisible(true);
    dateTimePicker.SetTheme(&focusRingDateTimeTheme);
    RecordingCanvas themedDateTimeCanvas;
    dateTimePicker.Paint(themedDateTimeCanvas, dateTimePicker.Bounds());
    assert(themedDateTimeCanvas.roundedStrokes == 3);
    dateTimePicker.SetFocused(false);
    dateTimePicker.SetMode(ysDui::controls::input::DuiDateTimePicker::Mode::Date);
    dateTimePicker.SetMinDate(DateTime{2024, 2, 10});
    dateTimePicker.SetMaxDate(DateTime{2024, 2, 25});
    assert(dateTimePicker.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 10})));
    assert(dateTimePicker.PopupOpen() && (dateTimePopup.options.size == Size{240, 230}));
    assert(dateTimePopup.Dispatch(ysDui::test::MakeEvent(EventType::PointerDown, {150, 66})));
    assert((dateTimePicker.Date() == DateTime{2024, 2, 10}));
    assert(dateTimePopup.requestedHide && !dateTimePicker.PopupOpen());
    dateTimePopup.requestedHide = false;
    assert(dateTimePicker.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Function1 + 3)));
    assert(dateTimePicker.PopupOpen());
    assert(dateTimePicker.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Escape)));
    assert(!dateTimePicker.PopupOpen());

    ysDui::controls::list::DuiPropertyGrid propertyGrid;
    propertyGrid.Layout({0, 0, 260, 140});
    TextInputMock propertyInput;
    propertyGrid.SetTextInput(&propertyInput);
    propertyGrid.AddGroup("General");
    const int propertyString = propertyGrid.AddString("Name", "Initial");
    const int propertyBool = propertyGrid.AddBool("Enabled", false);
    const int propertyInt = propertyGrid.AddInt("Count", 3);
    const int propertyEnum = propertyGrid.AddEnum("Mode", {{"First", 1}, {"Second", 2}}, 1);
    int changedProperty = -1;
    int editedProperty = -1;
    propertyGrid.SetValueChangedHandler([&changedProperty](int row) { changedProperty = row; });
    propertyGrid.SetEditedHandler([&editedProperty](int row) { editedProperty = row; });
    assert(propertyGrid.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 65})));
    assert(propertyGrid.BoolValueAt(propertyBool) && changedProperty == propertyBool);
    assert(propertyGrid.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, {10, 117})));
    assert(propertyGrid.EnumValueAt(propertyEnum) == 2 && changedProperty == propertyEnum);
    assert(propertyGrid.BeginEdit(propertyString) && propertyInput.visible);
    propertyInput.SetText("Updated");
    RecordingCanvas proxyPropertyCanvas;
    propertyGrid.Paint(proxyPropertyCanvas, propertyGrid.Bounds());
    assert(std::find(proxyPropertyCanvas.drawnTexts.begin(), proxyPropertyCanvas.drawnTexts.end(), "Updated")
           != proxyPropertyCanvas.drawnTexts.end());
    propertyGrid.CommitEdit();
    assert(propertyGrid.StringValueAt(propertyString) == "Updated" && editedProperty == propertyString);
    assert(propertyGrid.BeginEdit(propertyInt));
    propertyInput.SetText("42");
    propertyGrid.CommitEdit();
    assert(propertyGrid.IntValueAt(propertyInt) == 42 && editedProperty == propertyInt);
    assert(propertyGrid.BeginEdit(propertyInt));
    propertyInput.SetText("-2147483648");
    propertyGrid.CommitEdit();
    assert(propertyGrid.IntValueAt(propertyInt) == (std::numeric_limits<int>::min)());

    // DuiRangeSlider：双端区间，low <= high 且相互不交叉
    ysDui::controls::input::DuiRangeSlider rangeSlider;
    rangeSlider.SetRange(0, 100);
    rangeSlider.SetLineSize(1);
    int rangeLow{-1};
    int rangeHigh{-1};
    int rangeNotifications{};
    rangeSlider.SetValuesChangedHandler([&rangeLow, &rangeHigh, &rangeNotifications](int low, int high)
    {
        rangeLow = low;
        rangeHigh = high;
        ++rangeNotifications;
    });
    assert(rangeSlider.Low() == 0 && rangeSlider.High() == 100);
    assert(rangeSlider.ActiveThumb() == ysDui::controls::input::DuiRangeThumb::None);

    rangeSlider.SetValues(20, 80, true);
    assert(rangeSlider.Low() == 20 && rangeSlider.High() == 80);
    assert(rangeLow == 20 && rangeHigh == 80 && rangeNotifications == 1);

    // 传入颠倒区间自动交换
    rangeSlider.SetValues(90, 30, true);
    assert(rangeSlider.Low() == 30 && rangeSlider.High() == 90);
    // 越界钳制到范围
    rangeSlider.SetValues(-50, 500, true);
    assert(rangeSlider.Low() == 0 && rangeSlider.High() == 100);
    // 未变化时不重复回调
    const int notificationsBefore = rangeNotifications;
    rangeSlider.SetValues(0, 100, true);
    assert(rangeNotifications == notificationsBefore);

    rangeSlider.SetBounds({0, 0, 100, 20});
    // 轨道内缩 7，跨度 86
    const Rect rangeTrack = rangeSlider.TrackRect();
    assert(rangeTrack.left == 7 && rangeTrack.right == 93);
    // 端点坐标左右单调
    assert(rangeSlider.ThumbCenter(ysDui::controls::input::DuiRangeThumb::Low).x
           < rangeSlider.ThumbCenter(ysDui::controls::input::DuiRangeThumb::High).x);

    // 命中测试：按下下界拇指并拖到中点，上界不变
    rangeSlider.SetValues(0, 100, false);
    const Point lowThumb = rangeSlider.ThumbCenter(ysDui::controls::input::DuiRangeThumb::Low);
    assert(rangeSlider.ThumbFromPoint(lowThumb) == ysDui::controls::input::DuiRangeThumb::Low);
    assert(rangeSlider.ThumbFromPoint({1000, 1000}) == ysDui::controls::input::DuiRangeThumb::None);
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, lowThumb)));
    assert(rangeSlider.ActiveThumb() == ysDui::controls::input::DuiRangeThumb::Low);
    const Point rangeMid{(rangeTrack.left + rangeTrack.right) / 2, (rangeTrack.top + rangeTrack.bottom) / 2};
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, rangeMid)));
    assert(rangeSlider.Low() > 0 && rangeSlider.Low() < rangeSlider.High());
    assert(rangeSlider.High() == 100);
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, rangeMid)));
    assert(rangeSlider.ActiveThumb() == ysDui::controls::input::DuiRangeThumb::None);

    // 拖动不交叉：把下界拖到最右也不超过上界
    rangeSlider.SetValues(40, 60, false);
    const Point highThumb = rangeSlider.ThumbCenter(ysDui::controls::input::DuiRangeThumb::High);
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, highThumb)));
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, {1000, 10})));
    assert(rangeSlider.Low() <= rangeSlider.High());
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::PointerUp, {1000, 10})));

    // 键盘：未按住时作用于下界
    rangeSlider.SetValues(10, 50, false);
    assert(rangeSlider.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Right)));
    assert(rangeSlider.Low() == 11 && rangeSlider.High() == 50);

    // 绘制：区间填入最后一次圆角矩形，两个拇指各一个椭圆与描边。
    // low=11 / high=50，跨度 86：low.x = 7 + 86*11/100 = 16，high.x = 7 + 86*50/100 = 50
    RecordingCanvas rangeCanvas;
    rangeSlider.Paint(rangeCanvas, rangeSlider.Bounds());
    assert((rangeCanvas.rounded.bounds == Rect{16, 8, 50, 12}));
    assert(rangeCanvas.roundedRadius == 2);
    assert(rangeCanvas.ellipses.size() == 2);
    assert(rangeCanvas.strokeBoundsList.size() == 2);

    // DuiRatingControl：整星选择
    ysDui::controls::input::DuiRatingControl rating;
    assert(rating.MaxRating() == 5 && rating.Value() == 0 && !rating.ReadOnly());
    int ratedValue{-1};
    int ratingNotifications{};
    rating.SetValueChangedHandler([&ratedValue, &ratingNotifications](int value)
    {
        ratedValue = value;
        ++ratingNotifications;
    });
    rating.SetValue(3, true);
    assert(rating.Value() == 3 && ratedValue == 3 && ratingNotifications == 1);
    // 越界钳制
    rating.SetValue(99, true);
    assert(rating.Value() == 5);
    rating.SetValue(-5, true);
    assert(rating.Value() == 0);
    // 减小上限时当前值跟随钳制
    rating.SetValue(5, false);
    rating.SetMaxRating(3);
    assert(rating.MaxRating() == 3 && rating.Value() == 3);

    rating.SetStarSize(20);
    rating.SetStarGap(5);
    rating.SetBounds({0, 0, 200, 20});
    assert((rating.DesiredSize() == Size{3 * 20 + 2 * 5, 20}));
    // 星位从左到右等距排列
    const Rect firstStar = rating.StarRect(0);
    const Rect secondStar = rating.StarRect(1);
    assert((firstStar == Rect{0, 0, 20, 20}));
    assert((secondStar == Rect{25, 0, 45, 20}));
    assert(rating.StarRect(9).Empty());

    // 悬停预览：移到第二颗星时显示值变为 2，但实际取值不变
    const Point secondStarCenter{(secondStar.left + secondStar.right) / 2,
                                 (secondStar.top + secondStar.bottom) / 2};
    assert(rating.StarFromPoint(secondStarCenter) == 1);
    assert(rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, secondStarCenter)));
    assert(rating.DisplayedValue() == 2 && rating.Value() == 3);
    // 移出后预览复位
    assert(!rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerLeave, {-10, -10})));
    assert(rating.DisplayedValue() == 3);

    // 点击第三颗星 -> 取值 3；再点同一颗 -> 取消为 0
    const Rect thirdStar = rating.StarRect(2);
    const Point thirdStarCenter{(thirdStar.left + thirdStar.right) / 2,
                                (thirdStar.top + thirdStar.bottom) / 2};
    rating.SetValue(0, false);
    assert(rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, thirdStarCenter)));
    assert(rating.Value() == 3);
    assert(rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, thirdStarCenter)));
    assert(rating.Value() == 0);

    // 只读时不响应点击与键盘
    rating.SetReadOnly(true);
    assert(rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerMove, thirdStarCenter)));
    assert(!rating.OnEvent(ysDui::test::MakeEvent(EventType::PointerDown, thirdStarCenter)));
    assert(rating.Value() == 0);
    assert(!rating.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Right)));
    rating.SetReadOnly(false);
    assert(rating.OnEvent(ysDui::test::MakeEvent(EventType::KeyDown, {}, ysDui::core::key::Right)));
    assert(rating.Value() == 1);

    // 绘制：三颗星各自填充与描边
    RecordingCanvas ratingCanvas;
    rating.Paint(ratingCanvas, rating.Bounds());
    assert(ratingCanvas.filledPaths.size() == 3);
}
