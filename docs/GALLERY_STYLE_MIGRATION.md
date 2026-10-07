# Gallery Style Migration Matrix

Source baseline: `D:/00-ClionCode/balloonui-main` commit `d1ab3f3`,
`example/DuiGallery/Pages.cpp`.

The source Gallery is the visual and interaction specification. A v2 page is
complete only when it has a dedicated builder and exercises the corresponding
portable control capabilities. `Dedicated` records that a named builder exists
and is covered by the catalog construction test; it is not by itself visual
baseline approval. `Standard` means the current generic page is not acceptable
as final coverage.

| Source page | v2 page/control | Required capability | Status |
| --- | --- | --- | --- |
| Button | DuiButton | variants, icon, radio, pointer/keyboard states, focus ring, text styles and nine-patch skins | Dedicated |
| CheckBox | DuiCheckBox | click/keyboard toggle, Default/Ghost/Outlined/Text chrome, disabled state | Dedicated |
| DropDownButton | DuiButton (`DropDown` / `Split`) | trailing chevron in a 22 px strip, region-aware activation (whole button versus primary plus drop-down), divider line and disabled state | Dedicated |
| Flyout | DuiFlyout | anchored content bubble over `IPopupHost`, content factory for repeated open/close, fixed-size override, custom layout handler and dismissal callback | Dedicated |
| Charts | DuiLineChart / DuiBarChart / DuiPieChart | auto or explicit value range with rounded ticks, value axis and grid, category labels, series legend, data points, bar value captions, donut inner ratio and slice percentage legend; sectors are polygon-approximated because the render protocol has no arc path command | Dedicated |
| NodeEditor | DuiNodeEditor / DuiNodeGraph | grid canvas with two-level grid, cursor-anchored zoom and pan, auto-fit that tracks content, node/pin/link rendering with bezier polyline approximation, node and pin widget slots, selectable pin shapes (circle/square/arrow), node shadows and gradient headers, link hover/connected highlight, optional per-link flow animation, node drag, box select, pin-to-pin linking with legality feedback, groups moved as a unit plus fit-to-contents, snapshot undo/redo, copy/paste, XML save/load with stable ids, and embedded per-node and per-pin controls | Dedicated |
| Avatar | DuiAvatar | initials, image, shapes, status, sizes | Dedicated |
| Label | DuiLabel | color, alignment, link, UTF-8 range selection and clipboard copy | Dedicated |
| Badge | DuiBadge | count, radius, leading dot, truncation | Dedicated |
| Toast | DuiToast | dark appearance, variants, timing, portable icons, host-triggered display; extended with typed light appearance, close button, edge placement and duration 0 for no auto-close | Dedicated |
| ToastCenter | DuiToastCenter | stacked messages from top or bottom edge with offset and gap, grouping of identical content with repeat-count badge, self-managed lifetime | Dedicated |
| Separator | DuiSeparator | horizontal, vertical, thickness, inset | Dedicated |
| Watermark | DuiWatermark | rectangular/hexagonal tiling, alpha, gap, offset, line spacing, stacked text lines, grayscale image content and rotate via baked tiles | Dedicated |
| GroupBox | DuiGroupBox | title, border, padding and nested portable content | Dedicated |
| Image | DuiImage | portable image source, Fit/Stretch/Fill scale modes | Dedicated |
| StatusBar | DuiStatusBar | panes, alignment, resize | Dedicated |
| Expander | DuiExpander | nested content, animation and stacked sections | Dedicated |
| ToolBar | DuiToolBar | commands, separators, stretch layout and interactive overflow feedback | Dedicated |
| Breadcrumb | DuiBreadcrumb | hierarchy, selection and leading truncation that preserves the destination segment | Dedicated |
| Segmented | DuiSegmentedControl | states and selection | Dedicated |
| Edit | DuiEditHost | placeholder, password, read-only, multi-line, bounded native input and lifecycle | Dedicated |
| RichEdit | DuiRichEditHost | multi-line, links, read-only, quote and file-card command interaction | Dedicated |
| MarkdownView | DuiMarkdownView | GFM subset, streaming throttle, code copy, sanitized links | Dedicated |
| ComboBox | DuiComboBox | read-only/editable text, popup selection, long list, arrow color | Dedicated |
| Slider | DuiSlider | range, stepped track input, disabled style, pointer/wheel/key interaction | Dedicated |
| Switch | DuiSwitch | 46x24 default geometry, four states, colors, 150ms EaseOutCubic, disabled fade, keyboard | Dedicated |
| ProgressBar | DuiProgressBar | percent or custom text, colors, vertical and marquee state | Dedicated |
| SearchBox | DuiSearchBox | placeholder, clear, read-only, bounded native input | Dedicated |
| SpinBox | DuiSpinBox | range, step, wrap, native input and disabled state | Dedicated |
| DoubleSpinBox | DuiDoubleSpinBox | precision, range, step, wrap and native input | Dedicated |
| ColorPicker | DuiColorPicker | swatch variants, preset palette and deferred chooser trigger | Dedicated |
| DateTimePicker | DuiDateTimePicker | stable UTF-8 date, time and date-time text with deferred popup | Dedicated |
| MonthCalendar | DuiMonthCalendar | month navigation, date selection and range constraints | Dedicated |
| HotKey | DuiHotKey | shortcut capture, clear, disabled and function-key states | Dedicated |
| ScrollBar | DuiScrollBar | range, page size, horizontal/vertical dragging and disabled state | Dedicated |
| PathEdit | DuiPathEdit | file/folder styles, native input and deferred picker trigger | Dedicated |
| FileDialog | DuiPathPicker | open, save and folder platform actions with deferred trigger | Dedicated |
| Tab | DuiTab | old five-section strip variants, close/dropdown hover, icons, width modes, keyboard wrap, focus, overflow and reorder | Dedicated |
| ListBox | DuiListBox | selection, scrollbar, checks, multi-select and reorder | Dedicated |
| VirtualList | DuiVirtualList | 10000 virtual rows, scrolling, selection and activation | Dedicated |
| ScrollView | DuiScrollView | 360px viewport, 30-band content, clipping, background, wheel/scrollbar interaction and move-safe callbacks | Dedicated |
| TabPage | DuiTabPage | selectable header strip, visible content switching and header-height variants | Dedicated |
| TreeView | DuiTreeView | expansion, selection, filtering, hover updates, sortable columns, status dots, node icons, muted icons, two-line nodes, checkbox, progress, hyperlink, icon and image cells, and scrollable hierarchy | Dedicated |
| DataGrid | DuiDataGrid | rows, columns, sort intent, checks, multi-select, scrolling and native-adapter editing | Dedicated |
| PropertyGrid | DuiPropertyGrid | grouped string, integer, Boolean and enum settings with native-adapter editing | Dedicated |
| Menu | DuiMenu | command popup, check, separator and disabled variants | Dedicated |
| MenuBar | DuiMenuBar | File, Options and View dropdown interaction | Dedicated |
| ToolTip | DuiToolTipManager | three 120 x 36 targets, 500 ms delayed hover, cursor offset, pale-yellow popup, switching, click dismissal and page-owned lifecycle | Dedicated |
| PopupHost | ui::IPopupHost | below/above/right placement, legacy sizes, work-area flipping and interactive portable content | Dedicated |
| LayeredHost | ui::ILayeredHost | 280x120 dark rounded per-pixel-alpha bubble with deferred creation | Dedicated |
| Emoji | DuiEmojiPanel | user-triggered IPopupHost grid, selection-to-close flow, old hover/press colors, cell-relative default font size and compact inline variant | Dedicated |
| BusyIndicator | DuiBusyIndicator | old 32 x 32 loading row, 1 s active ring animation registration, large variant and inactive state | Dedicated |
| Gif | DuiGif | portable frames, timed playback, pause and native-size/stretch previews | Dedicated |
| AsyncImageLoader | DuiAsyncImageLoader | background decode, explicit result polling and successful-path cache | Dedicated |
| NativeHost | DuiNativeHost | 360px site, deferred Attach/Detach demo, decoration option, status, layout, visibility and owned release lifecycle | Dedicated |
| Layout | DuiVBox/DuiHBox | horizontal/vertical fixed, weighted, padding, gap and margin layout | Dedicated |
| Flow | DuiFlow | eight fixed 72x28 tags, 4px padding, 8px gap, weighted width and responsive wrapping | Dedicated |
| Stack | DuiStack | horizontal and vertical fixed/weighted allocation | Dedicated |
| Splitter | DuiSplitter | vertical/horizontal drag, minimum sizes, pane rendering and value feedback | Dedicated |
| Dock | DuiDock | top/bottom/left/right/fill docking with padding and gap | Dedicated |
| MessageBox | DuiMessageBox | four message types, three-button, multiline and custom-result variants | Dedicated |
| Dialog | DuiDialog | collapsible/maximizable modal, no-button and modeless title-bar variants | Dedicated |
| EmbeddedHost | ui::IEmbeddedHost | backend-selected child/owned-popup mode, dismissal policy, live bounds and embedded content | Dedicated |
| FrameHost | ui::IFrameHost | dynamic caption buttons/icons and min/max size variants | Dedicated |
| LoginXml | DuiXmlBuilder | XML-built portable control tree with flexible and fixed linear-layout child sizing | Dedicated |
| RichDocument | DuiRichDocument | text, quote, file and resource-image blocks with JSON persistence | Dedicated |
| ResourcePackage | DuiResourcePackage | user-triggered ZIP opening and portable UTF-8 entry inspection | Dedicated |
| Theme | DuiTheme | page-local light, dark and high-contrast control previews | Dedicated |
| A11y | core accessibility | roles, names and keyboard-focus semantics | Dedicated |
| DocCaptures | Gallery test tooling | page-level visual fixtures plus `--capture-all` PNG output for every dedicated page | Dedicated |
| Layouts | DuiHBox, DuiVBox, DuiDock | login, profile form, three-pane workspace, settings and window skeleton compositions | Dedicated |
| IconFont | DuiIconFont | Font Awesome glyph rendering, deployment and fallback | Dedicated |

## Post-Baseline Additions

These pages have no counterpart in the `d1ab3f3` source Gallery. They are
additions for the AI chat scenario, and the same rules apply: a dedicated
builder, a control-layer test, and a catalog structure test.

| Page | Control | Capability | Status |
| --- | --- | --- | --- |
| Chip | `DuiChip` | filled/outlined variants, theme or explicit colours, corner radius, padding, leading icon, close affordance, `DesiredSize()` and pill clipping | Dedicated |
| ChatBubble | `DuiChatBubble` | user/assistant/system role alignment and colouring, avatar column, meta line (name, timestamp, status text), max width and content-driven height | Dedicated |
| ChatList | `DuiChatList` | virtualized variable-height items, lazy measure at the viewport width, fixed height estimate for off-window items, window-only layout and paint, bottom-follow with user-scroll release, item invalidation for streamed growth, and **derived** unseen-item counting (append counts only while follow-bottom is off; reaching the bottom really zeroes it so old counts cannot reappear) | Dedicated |
| ImageViewer | `DuiImageViewer` | zoom anchored at the pointer, drag pan with "viewport always covered" clamping, Contain/Actual/Cover fit modes, wheel and double-click zoom, Escape and backdrop close **requests** (the control never hides itself), caption strip, transform-based drawing that only samples the visible source region | Dedicated |
| ChatStream | page-local `ChatStreamDemo` | the minimal chat page: Markdown messages built from `DuiMarkdownView` + `DuiChatBubble` inside a `DuiChatList`, with a replay button that drives a **background thread** producing chunks through `Host::Dispatcher()`, so streamed growth and bottom-follow are exercised through the real cross-thread channel. Teardown joins the producer and cancels its coalesce key. Also composes the back-to-bottom overlay from `DuiChatList::UnseenCount()` + `SetUnseenChangedHandler`, proving the affordance needs no new control. | Dedicated |
| TypingIndicator | `DuiTypingIndicator` | 3-dot pulse driven by `AnimationClock`, dot count/radius/gap, colour override and static inactive state | Dedicated |

`DuiScrollView` gained bottom-follow scrolling (`SetFollowBottom` /
`ScrollToBottom` / `AtBottom`) for streamed message append, and `DuiTheme`
gained the chat and chip slots listed in `PITFALLS.md`.

## Rules

- Every `Standard` row must be replaced with a named dedicated builder before
  the migration can be declared complete.
- A source-only Win32 concept is mapped to an existing platform capability
  interface; it must not reappear in a public ysDui header.
- Each completed row requires a control-layer test, a Gallery structure test,
  and a Windows backend test when platform behavior is involved.

## Baseline Decisions And Limits

- `DuiLabel` range selection is intentionally single-line only. This matches
  `d1ab3f3`: its word-wrapped labels are non-selectable. Cross-line selection
  requires a new text-layout product requirement rather than a fidelity fix.
- `Build_EmbeddedHost` exposed a raw anchor handle and let the Gallery choose
  between child and owned-popup modes. The approved v2 contract intentionally
  replaces that selector: `IUiHostFactory` supplies `IEmbeddedHost`, the backend
  selects its native mode from the anchor, and upper controls use only portable
  content, dismissal policy and `Rect` bounds.

## Fidelity Evidence

The Gallery capture command renders every page to `gallery-<page>.png` without
opening a window. CTest verifies the complete 96-page capture set against the
SHA-256 values in `tests/gallery_golden_sha256.txt`, so unintended Win32/GDI+
visual changes fail the regression suite. The following priority pages have
also been compared against the corresponding `d1ab3f3` builder; updating a
golden requires that manual source-baseline review first.

| Source builder | v2 page | Baseline capabilities checked | Evidence |
| --- | --- | --- | --- |
| `Build_Button` | Button | primary states, variants, icon, radio, keyboard activation, focus ring, text styles and nine-patch | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_CheckBox` | CheckBox | Default/Ghost/Outlined/Text chrome, pre-checked and disabled states, click toggle | `ysExeDuiGallery --capture-all` plus catalog and core interaction tests |
| `Build_Avatar` | Avatar | initials, blue/purple/orange gradient image sources, shapes, statuses, sizes | `ysdui_gallery --capture-all` plus catalog test |
| `Build_Label` | Label | color, alignment, link, visited and selection states | `ysdui_gallery --capture-all` plus catalog test |
| `Build_Badge` | Badge | red 1-99 counts, blue 99+ overflow, radius, Chinese status chips and truncation | `ysdui_gallery --capture-all` plus catalog test |
| `Build_Toast` | Toast | blue/green/orange/red semantic triggers, per-trigger duration reset, close and replacement | `ysdui_gallery --capture-all` plus catalog test |
| `Build_Separator` | Separator | orientation, color, thickness and inset | `ysdui_gallery --capture-all` plus catalog test |
| `Build_GroupBox` | GroupBox | title border, nested text and radio content | `ysdui_gallery --capture-all` plus catalog test |
| `Build_Expander` | Expander | accordion content and active/collapsed states | `ysdui_gallery --capture-all` plus catalog test |
| `Build_InfoBar` | InfoBar | four severities with accent stripe and icon, title plus body, close button and explicit-newline height | `ysdui_gallery --capture-all` plus catalog and core rendering tests |
| `Build_ComboBox` | ComboBox | read-only, editable, long-list and arrow-color variants | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Slider` | Slider | round thumb, 4px track, stepped range and disabled state | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_RangeSlider` | RangeSlider | dual thumbs, non-crossing clamp, stepped snapping, nearest-thumb wheel and disabled state | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_RatingControl` | RatingControl | star fill and outline, hover preview, click-to-clear, ten-star variant and read-only display | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_ProgressBar` | ProgressBar | percentage text, custom label contrast, palette, vertical and marquee states | `ysdui_gallery --capture-all` plus catalog and core rendering tests |
| `Build_ListBox` | ListBox | ten-row single selection, fifty-row automatic scrollbar, checks, multi-select and reorder | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_VirtualList` | VirtualList | 10000 rows, visible-row painting, selection, scrolling and first-row initial state | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_TreeView` | TreeView | hierarchy, sortable columns, hover updates, status dots, node icons, muted icons, right-side metadata, two-line nodes, checkbox, progress, hyperlink, icon and image cells | `ysdui_gallery --capture-all` plus catalog and core rendering tests |
| `Build_TabPage` | TabPage | page switching, 32/48 px headers, leading icons and clamped versus content-fit header widths | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_DataGrid` | DataGrid | sortable columns, checks, Ctrl/Shift selection, deferred native cell editing and scrolling rows | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_PropertyGrid` | PropertyGrid | grouped string, integer, Boolean and enum settings with deferred native value editing | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Menu` | Menu | plain commands, check state, disabled command, nested language menu and right-click context menu | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_MenuBar` | MenuBar | File, Options and View dropdowns; hover switching, mnemonics, separators and check states | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Stack` | Stack | page switching without tab chrome, selected-page visibility and shared page bounds | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Dock` | Dock | toolbar/status/nav/info/fill ordering, compact top-plus-fill pattern and gap allocation | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_Splitter` | Splitter | vertical and horizontal dragging, nested three-pane layout and minimum-size clamping | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Layout` | LinearLayout | fixed and weighted HBox/VBox children, margins, equal weights and fixed-column grid spanning | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_Layouts` | Layouts | login, form, three-pane workspace, settings and dock skeleton compositions with contrast-safe panel labels | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_UniformGrid` | UniformGrid | equal-width cells, explicit row cap, gap and padding plus hidden-child reflow | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_Canvas` | Canvas | absolute child placement, explicit versus DesiredSize sizing and insertion-order stacking | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_Image` | Image | portable generated image source, Fit and Stretch baseline modes plus Fill crop behavior | `ysdui_gallery --capture-all` plus catalog and core image geometry tests |
| `Build_StatusBar` | StatusBar | fixed and spring pane allocation, separators and color variants | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_ToolBar` | ToolBar | command buttons, separator, stretch allocation and overflow callback | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_SearchBox` | SearchBox | portable search/clear chrome, clear-region relayout, focused/disabled borders, Win32 text adapter attachment, Canvas text fallback, read-only and bounded input | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_SpinBox` | SpinBox | 120 x 23 default size, single shared input border, range, step, clamping, wrapping, arrow hover and press feedback | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_DoubleSpinBox` | DoubleSpinBox | 120 x 23 default size, single shared input border, decimals, range, step, clamping, wrapping, arrow hover and press feedback | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_ColorPicker` | ColorPicker | 160 x 32 Gallery field, preset swatches, `More` entry with a single-character ellipsis, deferred chooser trigger and focus ring | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_DateTimePicker` | DateTimePicker | 160/120/200 x 28 Gallery variants, mode-specific desired width, drop separator and arrow, F4 activation and focus ring | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_FileDialog` | FileDialog | 110/110/130 x 32 action buttons, single-character ellipsis and deferred open/save/folder capability | `ysdui_gallery --capture-all` plus catalog test |
| `Build_PathEdit` | PathEdit | input and browse chrome, file/folder modes, deferred picker invocation, disabled state and offscreen text fallback | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_HotKey` | HotKey | focus capture, modifier combinations, clear command, function-key and disabled variants | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_EditHost` | EditHost | single-line, password, read-only, multi-line and disabled variants with native-input and Canvas text fallback | `ysdui_gallery --capture-all` plus core and Win32 text-input tests |
| `Build_RichEditHost` | RichEditHost | multi-line text, placeholders, links, read-only content and rich-command entry points with native-input and Canvas text fallback | `ysdui_gallery --capture-all` plus core and Win32 rich-text-input tests |
| `Build_MonthCalendar` | MonthCalendar | month navigation, constraints, selection and full one/two-digit day labels | `ysdui_gallery --capture-all` plus catalog and core rendering tests |
| `Build_XmlBuilder` | XmlBuilder | XML-generated login card, flexible and fixed linear layout allocation, UTF-8 labels and button content | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_ScrollBar` | ScrollBar | horizontal and vertical track interaction, thumb dragging, page-size and disabled variants | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Breadcrumb` | Breadcrumb | clickable hierarchy segments, current-page emphasis and leading truncation that preserves the destination | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Segmented` | Segmented | mutually exclusive pointer selection, callback reporting and theme color variants | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_Switch` | Switch | 46 x 24 geometry, four states, 4px knob inset, custom colors, 150ms EaseOutCubic, disabled fade and pointer/keyboard toggling | `ysdui_gallery --capture-all` plus catalog and core interaction tests |
| `Build_ToolTip` | ToolTip | three 120 x 36 targets, 500ms delay, cursor offset, pale-yellow popup, switching and dismissal | `ysdui_gallery --capture-all` plus catalog, core and Win32 popup tests |
| `Build_Emoji` | Emoji | deferred popup grid, inline compact variant, selection callback and legacy hover/press colors | `ysdui_gallery --capture-all` plus catalog, core and Win32 popup tests |
| `Build_BusyIndicator` | BusyIndicator | 32 x 32 loading row, large variant, inactive state and page-scoped animation | `ysdui_gallery --capture-all` plus catalog and core animation tests |
| `Build_Tab` | Tab | plain, closeable/dropdown, leading icon and two auto-fit strips plus keyboard wrapping, focus, overflow and drag-reorder feedback | `ysdui_gallery --capture-all` plus catalog and core interaction/rendering tests |
| `Build_Flow` | Flow | eight fixed-size tag buttons, 4px padding, 8px gap and responsive weighted-width wrapping | `ysdui_gallery --capture-all` plus catalog and core layout tests |
| `Build_ScrollView` | ScrollView | 30 colored content bands, responsive weighted width, 360px clipped viewport, background, wheel movement and scrollbar painting | `ysdui_gallery --capture-all` plus catalog and core interaction/rendering tests |
| `Build_MessageBox` | MessageBox | four semantic colors, Yes/No/Cancel, multiline content, custom result buttons and deferred popup creation | `ysdui_gallery --capture-all` plus catalog, core and Win32 popup tests |
| `Build_Dialog` | Dialog | 480x320 collapsible modal with settings and OK/Apply/Cancel, 440x260 maximizable modal, 360x160 no-button modal and 360x180 modeless variant | `ysdui_gallery --capture-all` plus Gallery interaction, core and Win32 popup tests |
| `Build_PopupHost` | PopupHost | 220x180 below, 240x180 above and 220x200 right placements, work-area flipping and three-item portable content | `ysdui_gallery --capture-all` plus Gallery interaction and Win32 placement tests |
| `Build_LayeredHost` | LayeredHost | one 180px trigger and a 280x120 dark 14px-radius per-pixel-alpha bubble | `ysdui_gallery --capture-all` plus Gallery rendering and Win32 layered-host tests |
| `Build_NativeHost` | NativeHost | 360px deferred native site, attach/detach actions, decoration option, status feedback and owned release lifecycle | `ysdui_gallery --capture-all` plus Gallery interaction and core lifecycle tests |
| `Build_FrameWindow` | FrameHost | deferred top-level frame, live standard-button presets, dynamic portable caption icons, custom callbacks and min/max drag-size presets | `ysdui_gallery --capture-all` plus Gallery interaction and Win32 frame-host tests |
| `Build_EmbeddedHost` | EmbeddedHost | deferred persistent/dismissible hosts, backend-selected native mode, dynamic portable bounds and page-owned cleanup | `ysdui_gallery --capture-all` plus Gallery interaction and Win32 owner/child mapping tests |
| `Build_Theme` | Theme | Light/Dark controls, High Contrast v2 preset and 13 live legacy palette swatches | `ysdui_gallery --capture-all` plus Gallery rendering and core theme tests |
| `Build_A11y` | A11y | button, switch, label and edit roles with name/description overrides and identifiers 9101..9104 | `ysdui_gallery --capture-all` plus Gallery, core and Win32 UIA property tests |
