/**
 * 文件名：xml_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证 XML 文档读取和控件树构建逻辑。
 */
#include "test_support.hpp"

int main()
{
    using namespace ysDui::core;
    ysDui::render::DuiXmlDocument xml;
    assert(xml.Load("<resources package=\"core\"><image id=\"logo\">logo.svg</image></resources>"));
    assert(xml.Loaded() && xml.RootName() == "resources");
    assert(xml.RootAttribute("package") == std::optional<std::string>{"core"});
    assert(xml.ChildText("image") == std::optional<std::string>{"logo.svg"});
    assert(xml.ChildAttribute("image", "id") == std::optional<std::string>{"logo"});
    assert(!xml.Load("<resources>"));
    assert(xml.RootName() == "resources");

    ysDui::controls::DuiXmlBuilder xmlBuilder;
    int customProperty{};
    xmlBuilder.RegisterControl("custom", [] { return std::make_unique<Control>(); },
        [&customProperty](Control&, std::string_view name, std::string_view value)
        {
            if (name != "value")
                return false;
            customProperty = std::stoi(std::string(value));
            return true;
        });
    xmlBuilder.SetResourceResolver([](std::string_view path) { return "package/" + std::string(path); });

    using ysDui::controls::DuiXmlErrorCode;
    const auto malformedXml = xmlBuilder.BuildWithResult("<vbox>\n<label></vbox>");
    assert(!malformedXml && malformedXml.error);
    assert(malformedXml.error->code == DuiXmlErrorCode::ParseError);
    assert(!malformedXml.error->message.empty() && malformedXml.error->line >= 1
        && malformedXml.error->column >= 1);

    const auto unknownElement = xmlBuilder.BuildWithResult("<vbox>\n  <missing/>\n</vbox>");
    assert(!unknownElement && unknownElement.error);
    assert(unknownElement.error->code == DuiXmlErrorCode::UnknownElement);
    assert(unknownElement.error->element == "missing" && unknownElement.error->attribute.empty());
    assert(unknownElement.error->line == 2 && unknownElement.error->column == 4);

    xmlBuilder.RegisterControl("broken", [] { return std::unique_ptr<Control>{}; });
    const auto failedCreator = xmlBuilder.BuildWithResult("<broken/>");
    assert(!failedCreator && failedCreator.error);
    assert(failedCreator.error->code == DuiXmlErrorCode::ControlCreationFailed
        && failedCreator.error->element == "broken");

    const auto invalidFrameRoot = xmlBuilder.BuildFrameWithResult("<vbox/>");
    assert(!invalidFrameRoot && invalidFrameRoot.error);
    assert(invalidFrameRoot.error->code == DuiXmlErrorCode::InvalidFrameRoot
        && invalidFrameRoot.error->element == "vbox");

    const auto invalidFrameContent = xmlBuilder.BuildFrameWithResult(
        "<frame-window><label/><button/></frame-window>");
    assert(!invalidFrameContent && invalidFrameContent.error);
    assert(invalidFrameContent.error->code == DuiXmlErrorCode::InvalidFrameContent
        && invalidFrameContent.error->element == "frame-window");

    const auto invalidFrameAttribute = xmlBuilder.BuildFrameWithResult(
        "<frame-window title-bar-height=\"invalid\"><label/></frame-window>");
    assert(!invalidFrameAttribute && invalidFrameAttribute.error);
    assert(invalidFrameAttribute.error->code == DuiXmlErrorCode::InvalidAttribute);
    assert(invalidFrameAttribute.error->element == "frame-window"
        && invalidFrameAttribute.error->attribute == "title-bar-height");

    const auto buildControl = [&xmlBuilder](std::string_view xml)
    {
        auto result = xmlBuilder.BuildWithResult(xml);
        assert(result);
        return std::move(result.control);
    };

    std::string frameImagePath;
    xmlBuilder.SetImageResolver([&frameImagePath](std::string_view path)
        {
            frameImagePath = std::string(path);
            return std::shared_ptr<const ysDui::render::DuiImage>{};
        });
    auto xmlFrameResult = xmlBuilder.BuildFrameWithResult(
        "<frame-window title=\"Main\" title-bar-height=\"42\" title-bar-transparent=\"true\" "
        "title-text-color=\"1,2,3,4\" caption-glyph-color=\"5,6,7\" has-min-button=\"false\" "
        "has-max-button=\"false\" has-close-button=\"true\" min-w=\"320\" min-h=\"240\" "
        "max-w=\"1280\" max-h=\"720\" resizable=\"false\" border-px=\"3\" bg-image=\"frame.png\" "
        "bg-src-insets=\"1,2,3,4\" bg-dst-insets=\"5\"><label text=\"Content\"/></frame-window>");
    const auto* xmlFrame = xmlFrameResult ? &xmlFrameResult.document : nullptr;
    assert(xmlFrame && xmlFrame->content && frameImagePath == "package/frame.png");
    assert(xmlFrame->options.title == "Main" && xmlFrame->options.titleBarHeight == 42
        && xmlFrame->options.titleBarTransparent && !xmlFrame->options.showMinimize && !xmlFrame->options.showMaximize);
    assert((xmlFrame->options.minimumSize == Size{320, 240}) && (xmlFrame->options.maximumSize == Size{1280, 720}));
    assert(!xmlFrame->options.resizable && xmlFrame->options.resizeBorder == 3
        && (xmlFrame->options.backgroundSourceInsets == ysDui::render::DuiNinePatchInsets{1, 2, 3, 4})
        && (xmlFrame->options.backgroundDestinationInsets == ysDui::render::DuiNinePatchInsets{5, 5, 5, 5}));
    assert(dynamic_cast<ysDui::controls::basic::DuiLabel*>(xmlFrame->content.get()));
    assert(!xmlBuilder.BuildFrameWithResult("<vbox><label text=\"Invalid\"/></vbox>"));
    assert(!xmlBuilder.BuildFrameWithResult("<frame-window><label/><button/></frame-window>"));
    std::string xmlImagePath;
    const auto xmlResolvedImage = ysDui::render::DuiImage::CreateBgra8Premultiplied(
        {2, 2}, std::vector<unsigned char>(16, 255));
    xmlBuilder.SetImageResolver([&xmlImagePath, xmlResolvedImage](std::string_view path) {
        xmlImagePath = path;
        return xmlResolvedImage;
    });
    assert(xmlBuilder.ResolveResourcePath("logo.svg") == "package/logo.svg");
    auto xmlRoot = buildControl(
        "<stack orientation=\"vertical\" padding=\"2\" gap=\"3\">"
        "<label text=\"Hello\" fixed-height=\"20\"/>"
        "<button text=\"Apply\" fixed-height=\"24\"/>"
        "<custom value=\"7\"/></stack>");
    auto* xmlStack = dynamic_cast<ysDui::controls::layout::DuiStack*>(xmlRoot.get());
    assert(xmlStack && xmlStack->GetOrientation() == ysDui::controls::layout::DuiStackOrientation::Vertical);
    xmlStack->Layout({0, 0, 100, 100});
    assert(xmlStack->Children().size() == 3 && customProperty == 7);
    const auto* xmlLabel = dynamic_cast<const ysDui::controls::basic::DuiLabel*>(xmlStack->Children()[0].get());
    const auto* xmlButton = dynamic_cast<const ysDui::controls::basic::DuiButton*>(xmlStack->Children()[1].get());
    assert(xmlLabel && xmlLabel->Text() == "Hello" && xmlButton && xmlButton->Text() == "Apply");
    assert((xmlStack->Children()[0]->Bounds() == Rect{2, 2, 98, 22}));
    auto xmlChoices = buildControl(
        "<vbox><checkbox text=\"Remember\" checked=\"true\"/><radio text=\"Direct\" radio-group=\"8\" checked=\"true\"/>"
        "<radio text=\"System\" radio-group=\"8\"/></vbox>");
    auto* xmlChoicesBox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlChoices.get());
    assert(xmlChoicesBox && xmlChoicesBox->Children().size() == 3);
    const auto* xmlCheckBox = dynamic_cast<const ysDui::controls::basic::DuiCheckBox*>(xmlChoicesBox->Children()[0].get());
    const auto* xmlRadio = dynamic_cast<const ysDui::controls::basic::DuiRadio*>(xmlChoicesBox->Children()[1].get());
    const auto* xmlButtonRadio = dynamic_cast<const ysDui::controls::basic::DuiRadio*>(xmlChoicesBox->Children()[2].get());
    assert(xmlCheckBox && xmlCheckBox->Checked());
    assert(xmlRadio && xmlRadio->RadioGroup() == 8 && xmlRadio->Checked());
    assert(xmlButtonRadio && xmlButtonRadio->RadioGroup() == 8 && !xmlButtonRadio->Checked());

    auto xmlComboBox = buildControl(
        "<combo-box editable=\"true\" incremental-search=\"true\" substring-search=\"true\" max-visible-items=\"5\" "
        "item-height=\"28\" selected-index=\"1\"><item text=\"One\" icon=\"one.png\"/>"
        "<option text=\"Two\"/></combo-box>");
    auto* xmlComboBoxControl = dynamic_cast<ysDui::controls::input::DuiComboBox*>(xmlComboBox.get());
    assert(xmlComboBoxControl && xmlComboBoxControl->Count() == 2 && xmlComboBoxControl->TextAt(1) == "Two"
        && xmlComboBoxControl->SelectedIndex() == 1 && xmlComboBoxControl->Editable()
        && xmlComboBoxControl->ItemIconAt(0) == xmlResolvedImage && xmlImagePath == "package/one.png");

    auto xmlListBox = buildControl(
        "<list-box row-height=\"28\" multi-select=\"true\" checkboxes=\"true\" selected-index=\"1\">"
        "<item text=\"One\" value=\"11\" checked=\"true\"/><option text=\"Two\" value=\"12\" icon=\"two.png\"/>"
        "</list-box>");
    auto* xmlListBoxControl = dynamic_cast<ysDui::controls::list::DuiListBox*>(xmlListBox.get());
    assert(xmlListBoxControl && xmlListBoxControl->Count() == 2 && xmlListBoxControl->RowHeight() == 28
        && xmlListBoxControl->MultiSelect() && xmlListBoxControl->CheckboxesVisible() && xmlListBoxControl->IsChecked(0)
        && xmlListBoxControl->SelectedIndex() == 1 && xmlListBoxControl->ValueAt(1) == 12
        && xmlListBoxControl->IconAt(1) == xmlResolvedImage && xmlImagePath == "package/two.png");
    assert(!xmlBuilder.BuildWithResult("<stack>"));
    auto xmlVBox = buildControl(
        "<vbox padding=\"1\" gap=\"2\"><label text=\"A\" fixed-height=\"10\"/>"
        "<label text=\"B\" weight=\"2\"/></vbox>");
    auto* vbox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlVBox.get());
    assert(vbox && vbox->Children().size() == 2);
    vbox->Layout({0, 0, 40, 40});
    assert((vbox->Children()[0]->Bounds() == Rect{1, 1, 39, 11}));
    assert((vbox->Children()[1]->Bounds() == Rect{1, 13, 39, 39}));

    auto xmlDock = buildControl(
        "<dock padding=\"1\" gap=\"2\"><label text=\"Top\" side=\"top\" fixed-height=\"10\"/>"
        "<label text=\"Left\" side=\"left\" fixed-width=\"20\"/><label text=\"Fill\"/></dock>");
    auto* xmlDockControl = dynamic_cast<ysDui::controls::layout::DuiDock*>(xmlDock.get());
    assert(xmlDockControl && xmlDockControl->Children().size() == 3
        && xmlDockControl->GetPadding().left == 1 && xmlDockControl->GetGap() == 2);
    xmlDockControl->Layout({0, 0, 100, 70});
    assert(xmlDockControl->GetDock(xmlDockControl->Children()[0].get()) == ysDui::controls::layout::DuiDockSide::Top);
    assert((xmlDockControl->Children()[0]->Bounds() == Rect{1, 1, 99, 11}));
    assert((xmlDockControl->Children()[1]->Bounds() == Rect{1, 13, 21, 69}));
    assert((xmlDockControl->Children()[2]->Bounds() == Rect{23, 13, 99, 69}));

    // 停靠管理器：每个子元素成为一个窗格，id / title / slot 取自属性
    auto xmlDockManager = buildControl(
        "<dock-manager strip-thickness=\"20\">"
        "<label id=\"editor\" title=\"Editor\" text=\"Editor body\"/>"
        "<label title=\"Output\" text=\"Output body\" slot=\"bottom\"/>"
        "<label id=\"extra\" text=\"Extra\"/>"
        "</dock-manager>");
    auto* xmlDockManagerControl =
        dynamic_cast<ysDui::controls::docking::DuiDockManager*>(xmlDockManager.get());
    assert(xmlDockManagerControl != nullptr);
    assert(xmlDockManagerControl->PaneCount() == 3);
    assert(xmlDockManagerControl->Contains("editor"));
    assert(xmlDockManagerControl->Contains("extra"));
    assert(xmlDockManagerControl->PaneIds()[0] == "editor");
    // 缺省 id 由构建器生成
    assert(xmlDockManagerControl->Contains("pane1"));
    assert(xmlDockManagerControl->PaneContent("editor") != nullptr);
    // 未给 title 时标题退回 id
    assert(xmlDockManagerControl->Tree().PaneTitle("extra") == "extra");
    // slot="bottom" 的窗格会切出底部栏，根因此成为水平分割
    assert(xmlDockManagerControl->Tree().Root().kind
           == ysDui::controls::docking::DuiDockNodeKind::Split);
    assert(xmlDockManagerControl->Tree().Root().orientation
           == ysDui::controls::docking::DuiDockOrientation::Horizontal);
    xmlDockManagerControl->Layout({0, 0, 300, 200});
    assert(xmlDockManagerControl->Children().size() == 1);

    auto xmlFlow = buildControl(
        "<wrap padding=\"1,2,3,4\" gap=\"2\"><label text=\"A\" fixed-width=\"10\" fixed-height=\"5\"/>"
        "<label text=\"B\" fixed-width=\"12\" fixed-height=\"6\" margin=\"1\"/></wrap>");
    auto* xmlFlowControl = dynamic_cast<ysDui::controls::layout::DuiFlow*>(xmlFlow.get());
    assert(xmlFlowControl && xmlFlowControl->Children().size() == 2
        && xmlFlowControl->GetPadding().top == 2 && xmlFlowControl->GetGap() == 2);
    const auto flowItem = xmlFlowControl->GetItem(xmlFlowControl->Children()[1].get());
    assert((flowItem.desiredSize == Size{12, 6} && flowItem.margin.left == 1 && flowItem.margin.bottom == 1));
    xmlFlowControl->Layout({0, 0, 30, 30});
    assert((xmlFlowControl->Children()[0]->Bounds() == Rect{1, 2, 11, 7}));
    assert((xmlFlowControl->Children()[1]->Bounds() == Rect{14, 3, 26, 9}));

    auto xmlSplitter = buildControl(
        "<splitter orientation=\"horizontal\" bar-thickness=\"6\" min-size-0=\"7\" min-size-1=\"8\" split-fraction=\"0.25\">"
        "<label text=\"First\"/><label text=\"Second\"/><label text=\"Ignored\"/></splitter>");
    auto* xmlSplitterControl = dynamic_cast<ysDui::controls::layout::DuiSplitter*>(xmlSplitter.get());
    assert(xmlSplitterControl
        && xmlSplitterControl->GetOrientation() == ysDui::controls::layout::DuiSplitterOrientation::Horizontal);
    assert(xmlSplitterControl->Children().size() == 2
        && xmlSplitterControl->GetPane(0) && xmlSplitterControl->GetPane(1));
    xmlSplitterControl->Layout({0, 0, 100, 80});
    assert(xmlSplitterControl->GetBarThickness() == 6 && xmlSplitterControl->SplitPixels() == 18);
    assert((xmlSplitterControl->GetPane(0)->Bounds() == Rect{0, 0, 100, 18}));
    assert((xmlSplitterControl->GetPane(1)->Bounds() == Rect{0, 24, 100, 80}));

    auto xmlScrollView = buildControl(
        "<scrollview scrollbar-width=\"12\"><label text=\"Scrollable\"/><button text=\"Ignored\"/></scrollview>");
    auto* xmlScrollViewControl = dynamic_cast<ysDui::controls::layout::DuiScrollView*>(xmlScrollView.get());
    assert(xmlScrollViewControl && xmlScrollViewControl->Content()
        && dynamic_cast<ysDui::controls::basic::DuiLabel*>(xmlScrollViewControl->Content()));
    assert(xmlScrollViewControl->Children().size() == 2);
    xmlScrollViewControl->SetContentSize({100, 200});
    xmlScrollViewControl->Layout({0, 0, 100, 100});
    assert((xmlScrollViewControl->Children()[0]->Bounds() == Rect{88, 0, 100, 100}));

    auto xmlScrollViewAlias = buildControl("<scrollview sb-width=\"15\"><label text=\"Alias\"/></scrollview>");
    auto* xmlScrollViewAliasControl = dynamic_cast<ysDui::controls::layout::DuiScrollView*>(xmlScrollViewAlias.get());
    assert(xmlScrollViewAliasControl && xmlScrollViewAliasControl->Content());
    xmlScrollViewAliasControl->SetContentSize({100, 200});
    xmlScrollViewAliasControl->Layout({0, 0, 100, 100});
    assert((xmlScrollViewAliasControl->Children()[0]->Bounds() == Rect{85, 0, 100, 100}));

    auto xmlExpander = buildControl(
        "<expander title=\"Details\" expanded=\"false\" title-strip-height=\"30\" padding=\"1,2,3,4\">"
        "<label text=\"First\"/><button text=\"Second\"/></expander>");
    auto* xmlExpanderControl = dynamic_cast<ysDui::controls::basic::DuiExpander*>(xmlExpander.get());
    assert(xmlExpanderControl && xmlExpanderControl->Title() == "Details" && !xmlExpanderControl->Expanded());
    assert(xmlExpanderControl->TitleStripHeight() == 30 && xmlExpanderControl->Padding().bottom == 4);
    auto* xmlExpanderContent = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlExpanderControl->Content());
    assert(xmlExpanderContent && xmlExpanderContent->Children().size() == 2);

    auto xmlBusy = buildControl("<vbox><busy/><spinner active=\"false\"/></vbox>");
    auto* xmlBusyBox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlBusy.get());
    assert(xmlBusyBox && xmlBusyBox->Children().size() == 2);
    const auto* activeBusy = dynamic_cast<const ysDui::controls::feedback::DuiBusyIndicator*>(xmlBusyBox->Children()[0].get());
    const auto* inactiveBusy = dynamic_cast<const ysDui::controls::feedback::DuiBusyIndicator*>(xmlBusyBox->Children()[1].get());
    assert(activeBusy && activeBusy->Active() && inactiveBusy && !inactiveBusy->Active());

    auto xmlColorPicker = buildControl("<color-picker value=\"#2D6CDF\"/>");
    auto* xmlColorPickerControl = dynamic_cast<ysDui::controls::input::DuiColorPicker*>(xmlColorPicker.get());
    assert(xmlColorPickerControl && (xmlColorPickerControl->Color() == Color{45, 108, 223, 255}));
    auto xmlColorPickerRgb = buildControl("<colorpicker color=\"1,2,3,4\"/>");
    auto* xmlColorPickerRgbControl = dynamic_cast<ysDui::controls::input::DuiColorPicker*>(xmlColorPickerRgb.get());
    assert(xmlColorPickerRgbControl && (xmlColorPickerRgbControl->Color() == Color{1, 2, 3, 4}));

    auto xmlPathEdit = buildControl("<path-edit mode=\"folder\" value=\"C:/workspace\"/>");
    auto* xmlPathEditControl = dynamic_cast<ysDui::controls::input::DuiPathEdit*>(xmlPathEdit.get());
    assert(xmlPathEditControl && xmlPathEditControl->BrowseMode() == ysDui::ui::DuiPathPickerMode::Folder);
    assert(xmlPathEditControl->Path() == "C:/workspace");
    auto xmlPathEditFile = buildControl("<pathedit mode=\"file\" path=\"C:/input.txt\"/>");
    auto* xmlPathEditFileControl = dynamic_cast<ysDui::controls::input::DuiPathEdit*>(xmlPathEditFile.get());
    assert(xmlPathEditFileControl && xmlPathEditFileControl->BrowseMode() == ysDui::ui::DuiPathPickerMode::OpenFile);
    assert(xmlPathEditFileControl->Path() == "C:/input.txt");
    auto xmlPathEditSave = buildControl("<path-edit mode=\"save\" path=\"C:/output.txt\"/>");
    auto* xmlPathEditSaveControl = dynamic_cast<ysDui::controls::input::DuiPathEdit*>(xmlPathEditSave.get());
    assert(xmlPathEditSaveControl && xmlPathEditSaveControl->BrowseMode() == ysDui::ui::DuiPathPickerMode::SaveFile);
    assert(xmlPathEditSaveControl->Path() == "C:/output.txt");

    auto xmlNativeHost = buildControl(
        "<native-host preferred-width=\"120\" width=\"90\" height=\"40\" placeholder=\"false\"/>");
    auto* xmlNativeHostControl = dynamic_cast<ysDui::controls::window::DuiNativeHost*>(xmlNativeHost.get());
    assert(xmlNativeHostControl && (xmlNativeHostControl->PreferredSize() == Size{120, 40}));
    xmlNativeHostControl->Layout({0, 0, 120, 40});
    RecordingCanvas nativeHostCanvas;
    xmlNativeHostControl->Paint(nativeHostCanvas, {0, 0, 120, 40});
    assert(nativeHostCanvas.fills.empty());
    auto xmlNativeHostAlias = buildControl("<hwndhost preferredWidth=\"80\" preferredHeight=\"30\"/>");
    auto* xmlNativeHostAliasControl = dynamic_cast<ysDui::controls::window::DuiNativeHost*>(xmlNativeHostAlias.get());
    assert(xmlNativeHostAliasControl && (xmlNativeHostAliasControl->DesiredSize() == Size{80, 30}));

    auto xmlDateTime = buildControl(
        "<date-time value=\"2024-02-29 13:14:15\" mode=\"datetime\" format=\"%Y/%m/%d\"/>");
    auto* xmlDateTimeControl = dynamic_cast<ysDui::controls::input::DuiDateTimePicker*>(xmlDateTime.get());
    assert(xmlDateTimeControl && xmlDateTimeControl->GetMode() == ysDui::controls::input::DuiDateTimePicker::Mode::DateTime);
    assert((xmlDateTimeControl->Date() == DateTime{2024, 2, 29, 13, 14, 15}));
    assert(xmlDateTimeControl->Format() == "%Y/%m/%d");
    auto xmlTimePicker = buildControl("<datetimepicker mode=\"time\" value=\"08:09\"/>");
    auto* xmlTimePickerControl = dynamic_cast<ysDui::controls::input::DuiDateTimePicker*>(xmlTimePicker.get());
    assert(xmlTimePickerControl && xmlTimePickerControl->Date().hour == 8 && xmlTimePickerControl->Date().minute == 9);

    auto xmlHotKey = buildControl("<hotkey vk=\"83\" modifiers=\"3\"/>");
    auto* xmlHotKeyControl = dynamic_cast<ysDui::controls::input::DuiHotKey*>(xmlHotKey.get());
    assert(xmlHotKeyControl && xmlHotKeyControl->Key() == 83 && xmlHotKeyControl->Modifiers() == 3);
    auto xmlHotKeyMods = buildControl("<hotkey vk=\"65\" mods=\"2\" modifiers=\"3\"/>");
    auto* xmlHotKeyModsControl = dynamic_cast<ysDui::controls::input::DuiHotKey*>(xmlHotKeyMods.get());
    assert(xmlHotKeyModsControl && xmlHotKeyModsControl->Modifiers() == 2);

    auto xmlPropertyGrid = buildControl("<property-grid name-width=\"150\" row-height=\"28\"/>");
    auto* xmlPropertyGridControl = dynamic_cast<ysDui::controls::list::DuiPropertyGrid*>(xmlPropertyGrid.get());
    assert(xmlPropertyGridControl && xmlPropertyGridControl->NameColumnWidth() == 150 && xmlPropertyGridControl->RowHeight() == 28);

    auto xmlDataGrid = buildControl(
        "<data-grid row-height=\"30\" header-height=\"24\" show-checkboxes=\"true\" checkboxes=\"false\""
        " editable=\"true\" multi-select=\"true\" multiselect=\"false\">"
        "<column title=\"Name\" width=\"160\" min-width=\"80\" align=\"center\"/>"
        "<column title=\"Value\" align=\"right\" sortable=\"false\"/></data-grid>");
    auto* xmlDataGridControl = dynamic_cast<ysDui::controls::list::DuiDataGrid*>(xmlDataGrid.get());
    assert(xmlDataGridControl && xmlDataGridControl->RowHeight() == 30 && xmlDataGridControl->HeaderHeight() == 24);
    assert(xmlDataGridControl->CheckboxesVisible() && xmlDataGridControl->Editable() && xmlDataGridControl->MultiSelect());
    assert(xmlDataGridControl->ColumnCount() == 2);
    const auto xmlFirstColumn = xmlDataGridControl->ColumnAt(0);
    const auto xmlSecondColumn = xmlDataGridControl->ColumnAt(1);
    assert(xmlFirstColumn.title == "Name" && xmlFirstColumn.width == 160 && xmlFirstColumn.minimumWidth == 80);
    assert(xmlFirstColumn.alignment == ysDui::render::DuiTextAlignment::Center && xmlFirstColumn.sortable);
    assert(xmlSecondColumn.alignment == ysDui::render::DuiTextAlignment::End && !xmlSecondColumn.sortable);

    auto xmlTreeView = buildControl(
        "<treeview row-height=\"30\" header-height=\"24\" indent=\"20\" editable=\"true\" zebra=\"true\" node-checks=\"true\" multi-select=\"true\" "
        "frozen-cols=\"1\" frozen-rows=\"2\"><column title=\"Name\" width=\"160\" min-width=\"80\" "
        "align=\"center\"/><column title=\"Value\" align=\"right\" sortable=\"false\" editable=\"false\"/>"
        "</treeview>");
    auto* xmlTreeViewControl = dynamic_cast<ysDui::controls::list::DuiTreeView*>(xmlTreeView.get());
    assert(xmlTreeViewControl && xmlTreeViewControl->RowHeight() == 30 && xmlTreeViewControl->HeaderHeight() == 24);
    assert(xmlTreeViewControl->Indent() == 20 && xmlTreeViewControl->Editable() && xmlTreeViewControl->Zebra()
        && xmlTreeViewControl->NodeChecksVisible() && xmlTreeViewControl->MultiSelect());
    assert(xmlTreeViewControl->FrozenColumns() == 1 && xmlTreeViewControl->FrozenRows() == 2 && xmlTreeViewControl->ColumnCount() == 2);
    const auto xmlTreeFirstColumn = xmlTreeViewControl->ColumnAt(0);
    const auto xmlTreeSecondColumn = xmlTreeViewControl->ColumnAt(1);
    assert(xmlTreeFirstColumn.title == "Name" && xmlTreeFirstColumn.width == 160 && xmlTreeFirstColumn.minimumWidth == 80);
    assert(xmlTreeFirstColumn.alignment == ysDui::render::DuiTextAlignment::Center && xmlTreeSecondColumn.alignment == ysDui::render::DuiTextAlignment::End);
    assert(xmlTreeSecondColumn.sortable == false && xmlTreeSecondColumn.editable == false);
    const int xmlTreeRoot = xmlTreeViewControl->AddRoot("Project");
    xmlTreeViewControl->SetCellText(xmlTreeRoot, 1, "Ready");
    xmlTreeViewControl->Layout({0, 0, 240, 80});
    RecordingCanvas xmlTreeCanvas;
    xmlTreeViewControl->Paint(xmlTreeCanvas, {0, 0, 240, 80});
    assert(xmlTreeCanvas.drawnText == "Ready" && xmlTreeViewControl->HitTestId({10, 5}) == -1);

    auto xmlMenuBar = buildControl(
        "<menu-bar item-height=\"30\"><menu-item id=\"101\" text=\"&amp;File\"/>"
        "<menu-item id=\"102\" text=\"Edit\"/></menu-bar>");
    auto* xmlMenuBarControl = dynamic_cast<ysDui::controls::list::DuiMenuBar*>(xmlMenuBar.get());
    assert(xmlMenuBarControl && xmlMenuBarControl->ItemHeight() == 30 && xmlMenuBarControl->ItemCount() == 2);
    assert(xmlMenuBarControl->ItemAt(0).id == 101 && xmlMenuBarControl->ItemAt(0).text == "&File");
    assert(xmlMenuBarControl->ItemAt(1).id == 102 && xmlMenuBarControl->ItemAt(1).text == "Edit");

    auto xmlImage = buildControl("<image path=\"logo.png\" scale=\"fill\"/>");
    auto* xmlImageControl = dynamic_cast<ysDui::controls::media::DuiImage*>(xmlImage.get());
    assert(xmlImageControl && xmlImageControl->ScaleMode() == ysDui::controls::media::DuiImageScaleMode::Fill);
    assert(xmlImagePath == "package/logo.png");

    auto xmlToolBar = buildControl(
        "<toolbar><button id=\"21\" text=\"Open\"/><separator/><item id=\"22\" text=\"Save\"/>"
        "<spacer/><stretch/></toolbar>");
    auto* xmlToolBarControl = dynamic_cast<ysDui::controls::basic::DuiToolBar*>(xmlToolBar.get());
    assert(xmlToolBarControl && xmlToolBarControl->ItemCount() == 5);

    auto xmlEdit = buildControl("<edit placeholder=\"User name\" password=\"true\" multiline=\"true\"/>");
    auto* xmlEditControl = dynamic_cast<ysDui::controls::input::DuiEditHost*>(xmlEdit.get());
    TextInputMock xmlEditInput;
    assert(xmlEditControl);
    xmlEditControl->SetTextInput(&xmlEditInput);
    assert(xmlEditInput.placeholder == "User name" && xmlEditInput.options.password && xmlEditInput.options.multiline);

    auto xmlSearchBox = buildControl("<searchbox placeholder=\"Find\" read-only=\"true\" max-length=\"12\"/>");
    auto* xmlSearchBoxControl = dynamic_cast<ysDui::controls::input::DuiSearchBox*>(xmlSearchBox.get());
    TextInputMock xmlSearchInput;
    assert(xmlSearchBoxControl);
    xmlSearchBoxControl->SetTextInput(&xmlSearchInput);
    assert(xmlSearchInput.placeholder == "Find" && xmlSearchInput.options.readOnly && xmlSearchInput.options.maxLength == 12);

    auto xmlRichEdit = buildControl(
        "<richedit multi-line=\"false\" word-wrap=\"false\" placeholder=\"Notes\" read-only=\"true\" "
        "max-length=\"256\" auto-url-detect=\"true\"/>");
    auto* xmlRichEditControl = dynamic_cast<ysDui::controls::input::DuiRichEditHost*>(xmlRichEdit.get());
    RichTextInputMock xmlRichInput;
    assert(xmlRichEditControl);
    xmlRichEditControl->SetRichTextInput(&xmlRichInput);
    assert(xmlRichInput.placeholder == "Notes" && !xmlRichInput.options.multiline && !xmlRichInput.options.wordWrap);
    assert(xmlRichInput.options.readOnly && xmlRichInput.options.maxLength == 256 && xmlRichInput.automaticLinkDetection);

    auto xmlSpinBox = buildControl("<spinbox min=\"-5\" max=\"20\" step=\"3\" value=\"11\" wrap=\"true\"/>");
    auto* xmlSpinBoxControl = dynamic_cast<ysDui::controls::input::DuiSpinBox*>(xmlSpinBox.get());
    assert(xmlSpinBoxControl && xmlSpinBoxControl->Minimum() == -5 && xmlSpinBoxControl->Maximum() == 20 && xmlSpinBoxControl->Value() == 11);
    auto xmlDoubleSpinBox = buildControl("<double-spinbox min=\"-1.5\" max=\"3.5\" step=\"0.25\" decimals=\"3\" value=\"1.125\" wrap=\"true\"/>");
    auto* xmlDoubleSpinBoxControl = dynamic_cast<ysDui::controls::input::DuiDoubleSpinBox*>(xmlDoubleSpinBox.get());
    assert(xmlDoubleSpinBoxControl && xmlDoubleSpinBoxControl->Minimum() == -1.5 && xmlDoubleSpinBoxControl->Maximum() == 3.5);
    assert(xmlDoubleSpinBoxControl->Decimals() == 3 && xmlDoubleSpinBoxControl->Value() == 1.125);

    auto xmlControls = buildControl(
        "<vbox><avatar name=\"Ada Lovelace\" shape=\"rounded\" status=\"online\" fallback-bg-color=\"1,2,3\"/>"
        "<badge count=\"120\" bg-color=\"4,5,6\"/><separator orientation=\"vertical\" thickness=\"2\"/>"
        "<slider min=\"10\" max=\"20\" value=\"15\" vertical=\"true\"/><switch checked=\"true\"/>"
        "<progress min=\"5\" max=\"25\" value=\"10\" marquee=\"true\"/></vbox>");
    auto* controlsBox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlControls.get());
    assert(controlsBox && controlsBox->Children().size() == 6);
    const auto* xmlAvatar = dynamic_cast<const ysDui::controls::basic::DuiAvatar*>(controlsBox->Children()[0].get());
    const auto* xmlBadge = dynamic_cast<const ysDui::controls::basic::DuiBadge*>(controlsBox->Children()[1].get());
    const auto* xmlSeparator = dynamic_cast<const ysDui::controls::basic::DuiSeparator*>(controlsBox->Children()[2].get());
    const auto* xmlSlider = dynamic_cast<const ysDui::controls::input::DuiSlider*>(controlsBox->Children()[3].get());
    const auto* xmlSwitch = dynamic_cast<const ysDui::controls::input::DuiSwitch*>(controlsBox->Children()[4].get());
    const auto* xmlProgress = dynamic_cast<const ysDui::controls::feedback::DuiProgressBar*>(controlsBox->Children()[5].get());
    assert(xmlAvatar && xmlAvatar->Name() == "Ada Lovelace" && xmlAvatar->Shape() == ysDui::controls::basic::DuiAvatarShape::RoundedRectangle);
    assert((xmlAvatar->Status() == ysDui::controls::basic::DuiAvatarStatus::Online && xmlAvatar->FallbackColor() == Color{1, 2, 3, 255}));
    assert((xmlBadge && xmlBadge->Text() == "99+" && xmlBadge->BackgroundColor() == Color{4, 5, 6, 255}));
    assert(xmlSeparator && xmlSeparator->GetOrientation() == ysDui::controls::basic::DuiSeparator::Orientation::Vertical && xmlSeparator->GetThickness() == 2);

    // 带文本的分割线：text / align / dashed / gap，且 layout 是 orientation 的别名
    auto xmlTitledSeparator = buildControl(
        "<separator layout=\"horizontal\" text=\"TDesign\" align=\"left\" dashed=\"true\" gap=\"12\""
        " thickness=\"2\" inset=\"4\"/>");
    const auto* xmlTitled = dynamic_cast<const ysDui::controls::basic::DuiSeparator*>(
        xmlTitledSeparator.get());
    assert(xmlTitled != nullptr);
    assert(xmlTitled->Text() == "TDesign");
    assert(xmlTitled->TextAlign() == ysDui::controls::basic::DuiSeparatorTextAlign::Left);
    assert(xmlTitled->Dashed() && xmlTitled->TextGap() == 12);
    assert(xmlTitled->GetOrientation() == ysDui::controls::basic::DuiSeparator::Orientation::Horizontal);
    assert(xmlTitled->GetThickness() == 2 && xmlTitled->GetInset() == 4);
    auto xmlRightSeparator = buildControl("<separator text=\"End\" align=\"right\"/>");
    const auto* xmlRight = dynamic_cast<const ysDui::controls::basic::DuiSeparator*>(
        xmlRightSeparator.get());
    assert(xmlRight != nullptr
        && xmlRight->TextAlign() == ysDui::controls::basic::DuiSeparatorTextAlign::Right
        && !xmlRight->Dashed());

    // 可勾选分组框：checkable / checked
    auto xmlGroupBox = buildControl(
        "<group-box title=\"Network\" checkable=\"true\"><label text=\"Proxy\"/></group-box>");
    auto* xmlGroupBoxControl = dynamic_cast<ysDui::controls::basic::DuiGroupBox*>(xmlGroupBox.get());
    assert(xmlGroupBoxControl != nullptr);
    assert(xmlGroupBoxControl->Title() == "Network" && xmlGroupBoxControl->Checkable());
    assert(xmlGroupBoxControl->Checked());
    assert(xmlGroupBoxControl->Content() != nullptr);
    auto xmlUncheckedGroupBox = buildControl(
        "<group-box title=\"Off\" checkable=\"true\" checked=\"false\"><label text=\"Disabled\"/></group-box>");
    auto* xmlUnchecked = dynamic_cast<ysDui::controls::basic::DuiGroupBox*>(
        xmlUncheckedGroupBox.get());
    assert(xmlUnchecked != nullptr && !xmlUnchecked->Checked());
    assert(xmlUnchecked->Content() != nullptr && !xmlUnchecked->Content()->Enabled());
    // 未声明 checkable 时不显示复选框
    auto xmlPlainGroupBox = buildControl("<group-box title=\"Plain\"/>");
    auto* xmlPlain = dynamic_cast<ysDui::controls::basic::DuiGroupBox*>(xmlPlainGroupBox.get());
    assert(xmlPlain != nullptr && !xmlPlain->Checkable());
    assert(xmlSlider && xmlSlider->Minimum() == 10 && xmlSlider->Maximum() == 20 && xmlSlider->Value() == 15);
    assert(xmlSwitch && xmlSwitch->Checked());
    assert(xmlProgress && xmlProgress->Minimum() == 5 && xmlProgress->Maximum() == 25 && xmlProgress->Value() == 10 && xmlProgress->Marquee());

    auto xmlNewControls = buildControl(
        "<vbox><card title=\"Title\" subtitle=\"Subtitle\" padding=\"10,11,12,13\"><label text=\"Body\"/></card>"
        "<skeleton lines=\"4\" line-height=\"9\" gap=\"3\" line-width-percent=\"75\"/>"
        "<pagination pages=\"8\" page=\"3\" max-visible-pages=\"5\"/></vbox>");
    auto* newControlsBox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlNewControls.get());
    assert(newControlsBox && newControlsBox->Children().size() == 3);
    const auto* xmlCard = dynamic_cast<const ysDui::controls::basic::DuiCard*>(newControlsBox->Children()[0].get());
    const auto* xmlSkeleton = dynamic_cast<const ysDui::controls::feedback::DuiSkeleton*>(newControlsBox->Children()[1].get());
    const auto* xmlPagination = dynamic_cast<const ysDui::controls::list::DuiPagination*>(newControlsBox->Children()[2].get());
    assert(xmlCard && xmlCard->Title() == "Title" && xmlCard->Subtitle() == "Subtitle"
        && xmlCard->Content() != nullptr && xmlCard->Padding().left == 10);
    assert(xmlSkeleton && xmlSkeleton->LineCount() == 4 && xmlSkeleton->LineHeight() == 9
        && xmlSkeleton->Gap() == 3 && xmlSkeleton->LineWidthPercent() == 75);
    assert(xmlPagination && xmlPagination->PageCount() == 8 && xmlPagination->CurrentPage() == 2
        && xmlPagination->MaxVisiblePages() == 5);

    auto xmlCollections = buildControl(
        "<vbox><status-bar><pane width=\"40\" text=\"Ready\"/><pane spring=\"true\" text=\"Online\"/></status-bar>"
        "<breadcrumb path=\"Home / Settings\"><item text=\"Network\"/></breadcrumb>"
        "<segmented current=\"1\"><segment text=\"Day\"/><item text=\"Week\"/></segmented></vbox>");
    auto* collectionsBox = dynamic_cast<ysDui::controls::layout::DuiVBox*>(xmlCollections.get());
    assert(collectionsBox && collectionsBox->Children().size() == 3);
    const auto* xmlStatusBar = dynamic_cast<const ysDui::controls::basic::DuiStatusBar*>(collectionsBox->Children()[0].get());
    const auto* xmlBreadcrumb = dynamic_cast<const ysDui::controls::basic::DuiBreadcrumb*>(collectionsBox->Children()[1].get());
    const auto* xmlSegmented = dynamic_cast<const ysDui::controls::basic::DuiSegmentedControl*>(collectionsBox->Children()[2].get());
    assert(xmlStatusBar && xmlStatusBar->PaneCount() == 2 && xmlStatusBar->PaneText(1) == "Online");
    assert((xmlBreadcrumb && xmlBreadcrumb->Items() == std::vector<std::string>{"Home", "Settings", "Network"}));
    assert(xmlSegmented && xmlSegmented->SegmentCount() == 2 && xmlSegmented->SegmentText(1) == "Week" && xmlSegmented->SelectedIndex() == 1);

}
