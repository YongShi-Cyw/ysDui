/**
 * 文件名：DuiXmlBuilder.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：使用私有 pugixml 解析器构建平台无关 XML 控件树。
 */
#include "ysDui/controls/DuiXmlBuilder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "pugixml.hpp"

#include "ysDui/controls/basic/DuiAvatar.hpp"
#include "ysDui/controls/basic/DuiBadge.hpp"
#include "ysDui/controls/basic/DuiBreadcrumb.hpp"
#include "ysDui/controls/basic/DuiButton.hpp"
#include "ysDui/controls/basic/DuiCard.hpp"
#include "ysDui/controls/basic/DuiCheckBox.hpp"
#include "ysDui/controls/basic/DuiChip.hpp"
#include "ysDui/controls/basic/DuiRadio.hpp"
#include "ysDui/controls/basic/DuiExpander.hpp"
#include "ysDui/controls/basic/DuiGroupBox.hpp"
#include "ysDui/controls/basic/DuiLabel.hpp"
#include "ysDui/controls/basic/DuiSegmentedControl.hpp"
#include "ysDui/controls/basic/DuiSeparator.hpp"
#include "ysDui/controls/basic/DuiStatusBar.hpp"
#include "ysDui/controls/basic/DuiToolBar.hpp"
#include "ysDui/controls/feedback/DuiProgressBar.hpp"
#include "ysDui/controls/feedback/DuiBusyIndicator.hpp"
#include "ysDui/controls/feedback/DuiSkeleton.hpp"
#include "ysDui/controls/input/DuiSlider.hpp"
#include "ysDui/controls/input/DuiSwitch.hpp"
#include "ysDui/controls/input/DuiColorPicker.hpp"
#include "ysDui/controls/input/DuiComboBox.hpp"
#include "ysDui/controls/input/DuiAutoSuggestBox.hpp"
#include "ysDui/controls/input/DuiDateTimePicker.hpp"
#include "ysDui/controls/input/DuiEditHost.hpp"
#include "ysDui/controls/input/DuiHotKey.hpp"
#include "ysDui/controls/input/DuiPathEdit.hpp"
#include "ysDui/controls/input/DuiRichEditHost.hpp"
#include "ysDui/controls/content/DuiMarkdownView.hpp"
#include "ysDui/controls/input/DuiSearchBox.hpp"
#include "ysDui/controls/input/DuiSpinBox.hpp"
#include "ysDui/controls/input/DuiDoubleSpinBox.hpp"
#include "ysDui/controls/layout/DuiLayout.hpp"
#include "ysDui/controls/docking/DuiDockManager.hpp"
#include "ysDui/controls/layout/DuiDock.hpp"
#include "ysDui/controls/layout/DuiFlow.hpp"
#include "ysDui/controls/layout/DuiScrollView.hpp"
#include "ysDui/controls/layout/DuiStack.hpp"
#include "ysDui/controls/layout/DuiSplitter.hpp"
#include "ysDui/controls/list/DuiDataGrid.hpp"
#include "ysDui/controls/list/DuiListBox.hpp"
#include "ysDui/controls/list/DuiMenuBar.hpp"
#include "ysDui/controls/list/DuiPropertyGrid.hpp"
#include "ysDui/controls/list/DuiPagination.hpp"
#include "ysDui/controls/list/DuiTreeView.hpp"
#include "ysDui/controls/media/DuiImage.hpp"
#include "ysDui/controls/window/DuiNativeHost.hpp"
#include "ysDui/controls/window/DuiNavigationView.hpp"

namespace ysDui::controls {
namespace {

DuiXmlError MakeXmlError(DuiXmlErrorCode code, std::string message, std::string_view xml,
                         std::ptrdiff_t sourceOffset, std::string element = {}, std::string attribute = {})
{
    const std::size_t offset = sourceOffset > 0
        ? (std::min)(static_cast<std::size_t>(sourceOffset), xml.size()) : 0;
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t index = 0; index < offset; ++index)
    {
        if (xml[index] == '\n')
        {
            ++line;
            column = 1;
        }
        else
            ++column;
    }
    return {code, std::move(message), std::move(element), std::move(attribute), offset, line, column};
}

std::ptrdiff_t NodeSourceOffset(pugi::xml_node node)
{
    return node ? node.offset_debug() : 0;
}

std::ptrdiff_t AttributeSourceOffset(std::string_view xml, pugi::xml_node node, std::string_view attribute)
{
    const std::ptrdiff_t nodeOffset = NodeSourceOffset(node);
    const std::size_t begin = nodeOffset > 0 ? static_cast<std::size_t>(nodeOffset) : 0;
    const std::size_t tagEnd = xml.find('>', begin);
    const std::size_t offset = xml.find(attribute, begin);
    return offset != std::string_view::npos && (tagEnd == std::string_view::npos || offset < tagEnd)
        ? static_cast<std::ptrdiff_t>(offset) : nodeOffset;
}

std::string Normalize(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::optional<bool> TryParseBoolean(std::string_view value)
{
    const std::string normalized = Normalize(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes")
        return true;
    if (normalized == "false" || normalized == "0" || normalized == "no")
        return false;
    return {};
}

bool ParseBoolean(std::string_view value)
{
    return TryParseBoolean(value).value_or(false);
}

std::optional<int> TryParseInteger(std::string_view value)
{
    int result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        return {};
    return result;
}

int ParseInteger(std::string_view value, int fallback)
{
    const auto parsed = TryParseInteger(value);
    return parsed ? *parsed : fallback;
}

std::optional<double> ParseDouble(std::string_view value)
{
    const std::string text(value);
    char* end{};
    const double result = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0')
        return {};
    return result;
}

controls::layout::DuiThickness ParseThickness(std::string_view value)
{
    std::array<int, 4> values{};
    std::size_t begin{};
    int count{};
    while (begin <= value.size() && count < 4)
    {
        const std::size_t comma = value.find(',', begin);
        const std::string_view part = value.substr(begin, comma == std::string_view::npos ? value.size() - begin : comma - begin);
        values[static_cast<std::size_t>(count++)] = ParseInteger(part, 0);
        if (comma == std::string_view::npos)
            break;
        begin = comma + 1;
    }
    if (count == 1)
        values = {values[0], values[0], values[0], values[0]};
    return {values[0], values[1], values[2], values[3]};
}

layout::DuiDockPadding ParseDockPadding(std::string_view value)
{
    const layout::DuiThickness thickness = ParseThickness(value);
    return {thickness.left, thickness.top, thickness.right, thickness.bottom};
}

layout::DuiFlowThickness ParseFlowThickness(std::string_view value)
{
    const layout::DuiThickness thickness = ParseThickness(value);
    return {thickness.left, thickness.top, thickness.right, thickness.bottom};
}

docking::DuiDockSlot ParseDockSlot(std::string_view value)
{
    if (value == "left")
        return docking::DuiDockSlot::Left;
    if (value == "right")
        return docking::DuiDockSlot::Right;
    if (value == "top")
        return docking::DuiDockSlot::Top;
    if (value == "bottom")
        return docking::DuiDockSlot::Bottom;
    return docking::DuiDockSlot::Center;
}

layout::DuiDockSide ParseDockSide(std::string_view value)
{
    const std::string side = Normalize(value);
    if (side == "top")
        return layout::DuiDockSide::Top;
    if (side == "bottom")
        return layout::DuiDockSide::Bottom;
    if (side == "left")
        return layout::DuiDockSide::Left;
    if (side == "right")
        return layout::DuiDockSide::Right;
    return layout::DuiDockSide::Fill;
}

std::optional<core::Color> ParseColor(std::string_view value)
{
    std::array<int, 4> components{0, 0, 0, 255};
    std::size_t begin{};
    int count{};
    while (begin <= value.size() && count < 4)
    {
        const std::size_t comma = value.find(',', begin);
        const std::string_view part = value.substr(begin, comma == std::string_view::npos ? value.size() - begin : comma - begin);
        int component{};
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), component);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size())
            return {};
        components[static_cast<std::size_t>(count++)] = (std::clamp)(component, 0, 255);
        if (comma == std::string_view::npos)
            break;
        begin = comma + 1;
    }
    if (count != 3 && count != 4)
        return {};
    return core::Color{static_cast<unsigned char>(components[0]), static_cast<unsigned char>(components[1]),
                       static_cast<unsigned char>(components[2]), static_cast<unsigned char>(components[3])};
}

std::vector<std::string> SplitText(std::string_view value, std::string_view delimiters)
{
    std::vector<std::string> result;
    std::size_t begin{};
    while (begin < value.size())
    {
        const std::size_t end = value.find_first_of(delimiters, begin);
        std::string_view item = value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin);
        const std::size_t first = item.find_first_not_of(" \t\r\n");
        if (first != std::string_view::npos)
        {
            item.remove_prefix(first);
            item.remove_suffix(item.size() - item.find_last_not_of(" \t\r\n") - 1);
            result.emplace_back(item);
        }
        if (end == std::string_view::npos)
            break;
        begin = end + 1;
    }
    return result;
}

bool ApplyCommonProperty(core::Control& control, std::string_view name, std::string_view value)
{
    const std::string property = Normalize(name);
    if (property == "name" || property == "id")
    {
        control.SetName(std::string(value));
        return true;
    }
    if (property == "visible")
    {
        control.SetVisible(ParseBoolean(value));
        return true;
    }
    if (property == "enabled")
    {
        control.SetEnabled(ParseBoolean(value));
        return true;
    }
    return false;
}

bool ApplyLayoutProperty(layout::DuiLayout& layout, std::string_view name, std::string_view value)
{
    const std::string property = Normalize(name);
    if (property == "padding")
    {
        const layout::DuiThickness padding = ParseThickness(value);
        layout.SetPadding(padding.left, padding.top, padding.right, padding.bottom);
        return true;
    }
    if (property == "gap")
    {
        layout.SetGap(ParseInteger(value, 0));
        return true;
    }
    return false;
}

} // namespace

class DuiXmlBuilder::Impl final
{
public:
    struct Entry final
    {
        ControlCreator creator;
        PropertyApplier applier;
    };

    std::map<std::string, Entry, std::less<>> controls;
    ResourceResolver resourceResolver;
    ImageResolver imageResolver;
};

DuiXmlBuilder::DuiXmlBuilder() : builder_(std::make_unique<Impl>())
{
    RegisterControl("vbox", [] { return std::make_unique<layout::DuiVBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            return ApplyLayoutProperty(static_cast<layout::DuiVBox&>(control), name, value);
        });
    RegisterControl("hbox", [] { return std::make_unique<layout::DuiHBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            return ApplyLayoutProperty(static_cast<layout::DuiHBox&>(control), name, value);
        });
    RegisterControl("grid", [] { return std::make_unique<layout::DuiGrid>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            return ApplyLayoutProperty(static_cast<layout::DuiGrid&>(control), name, value);
        });
    RegisterControl("stack", [] { return std::make_unique<layout::DuiStack>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& stack = static_cast<layout::DuiStack&>(control);
            const std::string property = Normalize(name);
            if (property == "orientation")
            {
                stack.SetOrientation(Normalize(value) == "horizontal"
                    ? layout::DuiStackOrientation::Horizontal : layout::DuiStackOrientation::Vertical);
                return true;
            }
            if (property == "padding")
            {
                stack.SetPadding(ParseThickness(value));
                return true;
            }
            if (property == "gap")
            {
                stack.SetGap(ParseInteger(value, 0));
                return true;
            }
            return false;
        });
    RegisterControl("dock-manager", [] { return std::make_unique<docking::DuiDockManager>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            if (Normalize(name) != "strip-thickness")
                return false;
            static_cast<docking::DuiDockManager&>(control)
                .SetAutoHideStripThickness(ParseInteger(value, 24));
            return true;
        });
    RegisterControl("dock", [] { return std::make_unique<layout::DuiDock>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& dock = static_cast<layout::DuiDock&>(control);
            const std::string property = Normalize(name);
            if (property == "padding")
            {
                dock.SetPadding(ParseDockPadding(value));
                return true;
            }
            if (property == "gap")
            {
                dock.SetGap(ParseInteger(value, 0));
                return true;
            }
            return false;
        });
    const auto registerFlow = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<layout::DuiFlow>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                auto& flow = static_cast<layout::DuiFlow&>(control);
                const std::string property = Normalize(name);
                if (property == "padding")
                {
                    flow.SetPadding(ParseFlowThickness(value));
                    return true;
                }
                if (property == "gap")
                {
                    flow.SetGap(ParseInteger(value, 0));
                    return true;
                }
                return false;
            });
    };
    registerFlow("flow");
    registerFlow("wrap");
    RegisterControl("splitter", [] { return std::make_unique<layout::DuiSplitter>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& splitter = static_cast<layout::DuiSplitter&>(control);
            const std::string property = Normalize(name);
            if (property == "orientation")
            {
                splitter.SetOrientation(Normalize(value) == "horizontal"
                    ? layout::DuiSplitterOrientation::Horizontal : layout::DuiSplitterOrientation::Vertical);
                return true;
            }
            if (property == "bar-thickness")
            {
                splitter.SetBarThickness(ParseInteger(value, splitter.GetBarThickness()));
                return true;
            }
            if (property == "min-size-0" || property == "min-size-1")
                return true;
            if (property == "split-px")
            {
                splitter.SetSplitPixels(ParseInteger(value, 100));
                return true;
            }
            if (property == "split-fraction")
            {
                if (const auto fraction = ParseDouble(value))
                    splitter.SetSplitFraction(*fraction);
                return true;
            }
            return false;
        });
    RegisterControl("scrollview", [] { return std::make_unique<layout::DuiScrollView>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            const std::string property = Normalize(name);
            if (property != "scrollbar-width" && property != "sb-width")
                return false;
            static_cast<layout::DuiScrollView&>(control).SetScrollBarWidth(ParseInteger(value, 17));
            return true;
        });
    RegisterControl("expander", [] { return std::make_unique<basic::DuiExpander>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& expander = static_cast<basic::DuiExpander&>(control);
            const std::string property = Normalize(name);
            if (property == "title")
            {
                expander.SetTitle(std::string(value));
                return true;
            }
            if (property == "expanded")
            {
                expander.SetExpanded(ParseBoolean(value));
                return true;
            }
            if (property == "title-strip-height")
            {
                expander.SetTitleStripHeight(ParseInteger(value, 28));
                return true;
            }
            if (property == "padding")
            {
                const layout::DuiThickness padding = ParseThickness(value);
                expander.SetPadding({padding.left, padding.top, padding.right, padding.bottom});
                return true;
            }
            return false;
        });
    RegisterControl("card", [] { return std::make_unique<basic::DuiCard>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& card = static_cast<basic::DuiCard&>(control);
            const std::string property = Normalize(name);
            if (property == "title") { card.SetTitle(std::string(value)); return true; }
            if (property == "subtitle") { card.SetSubtitle(std::string(value)); return true; }
            if (property == "padding")
            {
                const layout::DuiThickness padding = ParseThickness(value);
                card.SetPadding({padding.left, padding.top, padding.right, padding.bottom});
                return true;
            }
            if (property == "corner-radius") { card.SetCornerRadius(ParseInteger(value, 6)); return true; }
            if (property == "header-height") { card.SetHeaderHeight(ParseInteger(value, 40)); return true; }
            return false;
        });
    const auto registerBusy = [this](std::string_view element)
    {
        RegisterControl(std::string(element), []
            {
                auto busy = std::make_unique<feedback::DuiBusyIndicator>();
                busy->SetActive(true);
                return busy;
            },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                if (Normalize(name) != "active")
                    return false;
                static_cast<feedback::DuiBusyIndicator&>(control).SetActive(ParseBoolean(value));
                return true;
            });
    };
    registerBusy("busy");
    registerBusy("spinner");
    const auto registerColorPicker = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<input::DuiColorPicker>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                if (const std::string property = Normalize(name); property != "color" && property != "value")
                    return false;
                auto& picker = static_cast<input::DuiColorPicker&>(control);
                if (!value.empty() && value.front() == '#')
                    picker.SetColor(input::DuiColorPicker::ParseColorHex(std::string(value)));
                else if (const auto color = ParseColor(value))
                    picker.SetColor(*color);
                return true;
            });
    };
    registerColorPicker("colorpicker");
    registerColorPicker("color-picker");
    const auto registerPathEdit = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<input::DuiPathEdit>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                auto& pathEdit = static_cast<input::DuiPathEdit&>(control);
                const std::string property = Normalize(name);
                if (property == "mode")
                {
                    const std::string mode = Normalize(value);
                    pathEdit.SetBrowseMode(mode == "folder" || mode == "dir"
                        ? ui::DuiPathPickerMode::Folder
                        : mode == "save" ? ui::DuiPathPickerMode::SaveFile
                                         : ui::DuiPathPickerMode::OpenFile);
                    return true;
                }
                if (property == "path" || property == "value")
                {
                    pathEdit.SetPath(std::string(value));
                    return true;
                }
                return false;
            });
    };
    registerPathEdit("pathedit");
    registerPathEdit("path-edit");
    const auto registerNativeHost = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<window::DuiNativeHost>(); },
            [](core::Control&, std::string_view name, std::string_view)
            {
                const std::string property = Normalize(name);
                return property == "preferred-width" || property == "preferredwidth" || property == "width"
                    || property == "preferred-height" || property == "preferredheight" || property == "height"
                    || property == "placeholder";
            });
    };
    registerNativeHost("nativehost");
    registerNativeHost("native-host");
    registerNativeHost("hwndhost");
    const auto registerDateTime = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<input::DuiDateTimePicker>(); },
            [](core::Control&, std::string_view name, std::string_view)
            {
                const std::string property = Normalize(name);
                return property == "mode" || property == "format" || property == "value";
            });
    };
    registerDateTime("datetime");
    registerDateTime("datetimepicker");
    registerDateTime("date-time");
    RegisterControl("hotkey", [] { return std::make_unique<input::DuiHotKey>(); },
        [](core::Control&, std::string_view name, std::string_view)
        {
            const std::string property = Normalize(name);
            return property == "vk" || property == "mods" || property == "modifiers";
        });
    const auto registerPropertyGrid = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<list::DuiPropertyGrid>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                auto& grid = static_cast<list::DuiPropertyGrid&>(control);
                const std::string property = Normalize(name);
                if (property == "name-width")
                {
                    grid.SetNameColumnWidth(ParseInteger(value, 140));
                    return true;
                }
                if (property == "row-height")
                {
                    grid.SetRowHeight(ParseInteger(value, 26));
                    return true;
                }
                return false;
            });
    };
    registerPropertyGrid("propertygrid");
    registerPropertyGrid("property-grid");
    const auto registerDataGrid = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<list::DuiDataGrid>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                auto& grid = static_cast<list::DuiDataGrid&>(control);
                const std::string property = Normalize(name);
                if (property == "row-height")
                {
                    grid.SetRowHeight(ParseInteger(value, 28));
                    return true;
                }
                if (property == "header-height")
                {
                    grid.SetHeaderHeight(ParseInteger(value, 26));
                    return true;
                }
                if (property == "show-checkboxes" || property == "checkboxes")
                    return true;
                if (property == "editable")
                {
                    grid.SetEditable(ParseBoolean(value));
                    return true;
                }
                if (property == "multi-select" || property == "multiselect")
                    return true;
                return false;
            });
    };
    registerDataGrid("datagrid");
    registerDataGrid("data-grid");
    RegisterControl("treeview", [] { return std::make_unique<list::DuiTreeView>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& tree = static_cast<list::DuiTreeView&>(control);
            const std::string property = Normalize(name);
            if (property == "row-height") { tree.SetRowHeight(ParseInteger(value, 28)); return true; }
            if (property == "header-height") { tree.SetHeaderHeight(ParseInteger(value, 26)); return true; }
            if (property == "indent") { tree.SetIndent(ParseInteger(value, 18)); return true; }
            if (property == "editable") { tree.SetEditable(ParseBoolean(value)); return true; }
            if (property == "zebra") { tree.SetZebra(ParseBoolean(value)); return true; }
            if (property == "node-checks" || property == "node-checkboxes" || property == "checks")
            {
                tree.SetNodeChecksVisible(ParseBoolean(value));
                return true;
            }
            if (property == "multi-select" || property == "multiselect") { tree.SetMultiSelect(ParseBoolean(value)); return true; }
            if (property == "frozen-cols") { tree.SetFrozenColumns(ParseInteger(value, 0)); return true; }
            if (property == "frozen-rows") { tree.SetFrozenRows(ParseInteger(value, 0)); return true; }
            return false;
        });
    RegisterControl("menu-bar", [] { return std::make_unique<list::DuiMenuBar>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            if (Normalize(name) != "item-height")
                return false;
            static_cast<list::DuiMenuBar&>(control).SetItemHeight(ParseInteger(value, 24));
            return true;
        });
    RegisterControl("image", [] { return std::make_unique<media::DuiImage>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            if (Normalize(name) != "scale")
                return false;
            const std::string mode = Normalize(value);
            static_cast<media::DuiImage&>(control).SetScaleMode(mode == "none" ? media::DuiImageScaleMode::None
                : mode == "stretch" ? media::DuiImageScaleMode::Stretch
                : mode == "fill" ? media::DuiImageScaleMode::Fill : media::DuiImageScaleMode::Fit);
            return true;
        });
    RegisterControl("toolbar", [] { return std::make_unique<basic::DuiToolBar>(); });
    RegisterControl("edit", [] { return std::make_unique<input::DuiEditHost>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& edit = static_cast<input::DuiEditHost&>(control);
            const std::string property = Normalize(name);
            if (property == "placeholder")
            {
                edit.SetPlaceholder(std::string(value));
                return true;
            }
            ui::DuiTextInputOptions options = edit.Options();
            if (property == "password")
                options.password = ParseBoolean(value);
            else if (property == "multiline")
                options.multiline = ParseBoolean(value);
            else
                return false;
            edit.SetOptions(options);
            return true;
        });
    RegisterControl("searchbox", [] { return std::make_unique<input::DuiSearchBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& search = static_cast<input::DuiSearchBox&>(control);
            const std::string property = Normalize(name);
            if (property == "placeholder") { search.SetPlaceholder(std::string(value)); return true; }
            if (property == "read-only") { search.SetReadOnly(ParseBoolean(value)); return true; }
            if (property == "max-length") { search.SetMaxLength(ParseInteger(value, 0)); return true; }
            if (property == "glyph-strip-width") { search.SetGlyphStripWidth(ParseInteger(value, 24)); return true; }
            if (property == "clear-strip-width") { search.SetClearStripWidth(ParseInteger(value, 22)); return true; }
            return false;
        });
    RegisterControl("richedit", [] { return std::make_unique<input::DuiRichEditHost>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& richEdit = static_cast<input::DuiRichEditHost&>(control);
            const std::string property = Normalize(name);
            ui::DuiTextInputOptions options = richEdit.Options();
            if (property == "multi-line")
                options.multiline = ParseBoolean(value);
            else if (property == "word-wrap")
                options.wordWrap = ParseBoolean(value);
            else if (property == "read-only")
                options.readOnly = ParseBoolean(value);
            else if (property == "max-length")
                options.maxLength = ParseInteger(value, 0);
            else if (property == "placeholder")
            {
                richEdit.SetPlaceholder(std::string(value));
                return true;
            }
            else if (property == "auto-url-detect")
            {
                richEdit.SetAutomaticLinkDetection(ParseBoolean(value));
                return true;
            }
            else
                return false;
            richEdit.SetOptions(options);
            return true;
        });
    RegisterControl("spinbox", [] { return std::make_unique<input::DuiSpinBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& spin = static_cast<input::DuiSpinBox&>(control);
            const std::string property = Normalize(name);
            if (property == "min" || property == "max") { spin.SetRange(property == "min" ? ParseInteger(value, spin.Minimum()) : spin.Minimum(), property == "max" ? ParseInteger(value, spin.Maximum()) : spin.Maximum()); return true; }
            if (property == "step") { spin.SetStep(ParseInteger(value, 1)); return true; }
            if (property == "value") { spin.SetValue(ParseInteger(value, 0)); return true; }
            if (property == "wrap") { spin.SetWrap(ParseBoolean(value)); return true; }
            return false;
        });
    const auto registerDoubleSpin = [this](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<input::DuiDoubleSpinBox>(); },
            [](core::Control& control, std::string_view name, std::string_view value)
            {
                auto& spin = static_cast<input::DuiDoubleSpinBox&>(control);
                const std::string property = Normalize(name);
                const auto number = ParseDouble(value);
                if (property == "min" && number) { spin.SetRange(*number, spin.Maximum()); return true; }
                if (property == "max" && number) { spin.SetRange(spin.Minimum(), *number); return true; }
                if (property == "step" && number) { spin.SetStep(*number); return true; }
                if (property == "value" && number) { spin.SetValue(*number); return true; }
                if (property == "decimals" || property == "decimal") { spin.SetDecimals(ParseInteger(value, 2)); return true; }
                if (property == "wrap") { spin.SetWrap(ParseBoolean(value)); return true; }
                return false;
            });
    };
    registerDoubleSpin("doublespinbox");
    registerDoubleSpin("double-spinbox");
    RegisterControl("label", [] { return std::make_unique<basic::DuiLabel>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& label = static_cast<basic::DuiLabel&>(control);
            const std::string property = Normalize(name);
            if (property == "text")
            {
                label.SetText(std::string(value));
                return true;
            }
            if (property == "word-wrap")
            {
                label.SetWordWrap(ParseBoolean(value));
                return true;
            }
            return false;
        });
    RegisterControl("markdownview", [] { return std::make_unique<content::DuiMarkdownView>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& view = static_cast<content::DuiMarkdownView&>(control);
            const std::string property = Normalize(name);
            if (property == "content" || property == "text")
            {
                view.SetContent(std::string(value));
                return true;
            }
            if (property == "streaming")
            {
                view.SetStreaming(ParseBoolean(value));
                return true;
            }
            if (property == "sanitize")
            {
                view.SetSanitize(ParseBoolean(value));
                return true;
            }
            if (property == "code-copyable" || property == "codecopyable")
            {
                view.SetCodeCopyable(ParseBoolean(value));
                return true;
            }
            if (property == "enable-table" || property == "enabletable")
            {
                view.SetEnableTable(ParseBoolean(value));
                return true;
            }
            return false;
        });
    const auto applyChoiceProperty = [](core::Control& control, std::string_view name, std::string_view value)
    {
        auto& button = static_cast<basic::DuiButton&>(control);
        const std::string property = Normalize(name);
        if (property == "text")
        {
            button.SetText(std::string(value));
            return true;
        }
        return false;
    };
    RegisterControl("button", [] { return std::make_unique<basic::DuiButton>(); },
        [applyChoiceProperty](core::Control& control, std::string_view name, std::string_view value)
        {
            if (applyChoiceProperty(control, name, value))
                return true;
            const std::string kind = Normalize(value);
            if (Normalize(name) != "kind")
                return false;
            static_cast<basic::DuiButton&>(control).SetKind(kind == "icon" ? basic::DuiButtonKind::Icon
                : kind == "dropdown" || kind == "drop-down" ? basic::DuiButtonKind::DropDown
                : kind == "split" ? basic::DuiButtonKind::Split
                : basic::DuiButtonKind::Push);
            return true;
        });
    const auto applyCheckBoxProperty = [](core::Control& control, std::string_view name, std::string_view value)
    {
        auto& checkBox = static_cast<basic::DuiCheckBox&>(control);
        const std::string property = Normalize(name);
        if (property == "text")
        {
            checkBox.SetText(std::string(value));
            return true;
        }
        if (property == "checked")
        {
            checkBox.SetChecked(ParseBoolean(value));
            return true;
        }
        return false;
    };
    const auto registerCheckBox = [this, applyCheckBoxProperty](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<basic::DuiCheckBox>(); },
            applyCheckBoxProperty);
    };
    registerCheckBox("checkbox");
    registerCheckBox("check-box");
    const auto applyRadioProperty = [](core::Control& control, std::string_view name, std::string_view value)
    {
        auto& radio = static_cast<basic::DuiRadio&>(control);
        const std::string property = Normalize(name);
        if (property == "text")
        {
            radio.SetText(std::string(value));
            return true;
        }
        if (property == "checked")
        {
            radio.SetChecked(ParseBoolean(value));
            return true;
        }
        if (property == "radio-group" || property == "radiogroup")
        {
            radio.SetRadioGroup(ParseInteger(value, 0));
            return true;
        }
        return false;
    };
    const auto registerRadio = [this, applyRadioProperty](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<basic::DuiRadio>(); },
            applyRadioProperty);
    };
    registerRadio("radio");
    registerRadio("radio-button");
    const auto applyComboBoxProperty = [](core::Control& control, std::string_view name, std::string_view value)
    {
        auto& comboBox = static_cast<input::DuiComboBox&>(control);
        const std::string property = Normalize(name);
        if (property == "editable") { comboBox.SetEditable(ParseBoolean(value)); return true; }
        if (property == "incremental-search") { comboBox.SetIncrementalSearch(ParseBoolean(value)); return true; }
        if (property == "substring-search") { comboBox.SetSubstringSearch(ParseBoolean(value)); return true; }
        if (property == "max-visible-items") { comboBox.SetMaxVisibleItems(ParseInteger(value, 8)); return true; }
        if (property == "item-height") { comboBox.SetItemHeight(ParseInteger(value, 22)); return true; }
        if (property == "text") { comboBox.SetText(std::string(value)); return true; }
        return false;
    };
    const auto registerComboBox = [this, applyComboBoxProperty](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<input::DuiComboBox>(); },
            applyComboBoxProperty);
    };
    registerComboBox("combobox");
    registerComboBox("combo-box");
    RegisterControl("auto-suggest", [] { return std::make_unique<input::DuiAutoSuggestBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            if (Normalize(name) != "text")
                return false;
            static_cast<input::DuiAutoSuggestBox&>(control).SetText(std::string(value), false);
            return true;
        });
    RegisterControl("autosuggest", [] { return std::make_unique<input::DuiAutoSuggestBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            if (Normalize(name) != "text")
                return false;
            static_cast<input::DuiAutoSuggestBox&>(control).SetText(std::string(value), false);
            return true;
        });
    const auto applyListBoxProperty = [](core::Control& control, std::string_view name, std::string_view value)
    {
        auto& listBox = static_cast<list::DuiListBox&>(control);
        const std::string property = Normalize(name);
        if (property == "row-height") { listBox.SetRowHeight(ParseInteger(value, 22)); return true; }
        if (property == "multi-select" || property == "multiselect")
        {
            listBox.SetMultiSelect(ParseBoolean(value));
            return true;
        }
        if (property == "checkboxes" || property == "show-checkboxes")
        {
            listBox.SetCheckboxesVisible(ParseBoolean(value));
            return true;
        }
        return false;
    };
    const auto registerListBox = [this, applyListBoxProperty](std::string_view element)
    {
        RegisterControl(std::string(element), [] { return std::make_unique<list::DuiListBox>(); },
            applyListBoxProperty);
    };
    registerListBox("listbox");
    registerListBox("list-box");
    RegisterControl("group-box", [] { return std::make_unique<basic::DuiGroupBox>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& groupBox = static_cast<basic::DuiGroupBox&>(control);
            const std::string property = Normalize(name);
            if (property == "title")
            {
                groupBox.SetTitle(std::string(value));
                return true;
            }
            if (property == "checkable")
            {
                groupBox.SetCheckable(ParseBoolean(value));
                return true;
            }
            if (property == "checked")
            {
                groupBox.SetChecked(ParseBoolean(value));
                return true;
            }
            return false;
        });
    RegisterControl("navigation-view", [] { return std::make_unique<window::DuiNavigationView>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& nav = static_cast<window::DuiNavigationView&>(control);
            const std::string property = Normalize(name);
            if (property == "mode" || property == "pane-mode")
            {
                nav.SetPaneDisplayMode(Normalize(value) == "compact" || Normalize(value) == "leftcompact"
                    ? window::DuiNavigationPaneDisplayMode::LeftCompact
                    : window::DuiNavigationPaneDisplayMode::Left);
                return true;
            }
            return false;
        });
    RegisterControl("avatar", [] { return std::make_unique<basic::DuiAvatar>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& avatar = static_cast<basic::DuiAvatar&>(control);
            const std::string property = Normalize(name);
            if (property == "name")
            {
                avatar.SetName(std::string(value));
                return true;
            }
            if (property == "fallback-bg-color")
            {
                if (const auto color = ParseColor(value))
                    avatar.SetFallbackColor(*color);
                return true;
            }
            if (property == "initials-color")
            {
                if (const auto color = ParseColor(value))
                {
                    auto style = avatar.InitialsStyle();
                    style.color = *color;
                    avatar.SetInitialsStyle(std::move(style));
                }
                return true;
            }
            if (property == "shape")
            {
                avatar.SetShape(Normalize(value) == "rounded" || Normalize(value) == "rounded-rectangle"
                    ? basic::DuiAvatarShape::RoundedRectangle : basic::DuiAvatarShape::Circle);
                return true;
            }
            if (property == "corner-radius")
            {
                avatar.SetCornerRadius(ParseInteger(value, 8));
                return true;
            }
            if (property == "status")
            {
                const std::string status = Normalize(value);
                const auto parsed = status == "online" ? basic::DuiAvatarStatus::Online
                    : status == "away" ? basic::DuiAvatarStatus::Away
                    : status == "busy" ? basic::DuiAvatarStatus::Busy
                    : status == "offline" ? basic::DuiAvatarStatus::Offline : basic::DuiAvatarStatus::None;
                avatar.SetStatus(parsed);
                return true;
            }
            return false;
        });
    RegisterControl("chip", [] { return std::make_unique<basic::DuiChip>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& chip = static_cast<basic::DuiChip&>(control);
            const std::string property = Normalize(name);
            if (property == "text")
            {
                chip.SetText(std::string(value));
                return true;
            }
            if (property == "variant")
            {
                chip.SetVariant(Normalize(value) == "outlined" ? basic::DuiChipVariant::Outlined
                                                               : basic::DuiChipVariant::Filled);
                return true;
            }
            if (property == "closable")
            {
                chip.SetClosable(ParseBoolean(value));
                return true;
            }
            if (property == "fill-color")
            {
                if (const auto color = ParseColor(value))
                    chip.SetFillColor(*color);
                return true;
            }
            if (property == "text-color")
            {
                if (const auto color = ParseColor(value))
                    chip.SetTextColor(*color);
                return true;
            }
            if (property == "border-color")
            {
                if (const auto color = ParseColor(value))
                    chip.SetBorderColor(*color);
                return true;
            }
            if (property == "corner-radius")
            {
                chip.SetCornerRadius(ParseInteger(value, -1));
                return true;
            }
            if (property == "padding")
            {
                const int horizontal = ParseInteger(value, 10);
                chip.SetPadding(horizontal, chip.VerticalPadding());
                return true;
            }
            return false;
        });
    RegisterControl("badge", [] { return std::make_unique<basic::DuiBadge>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& badge = static_cast<basic::DuiBadge&>(control);
            const std::string property = Normalize(name);
            if (property == "count")
            {
                badge.SetCount(ParseInteger(value, 0));
                return true;
            }
            if (property == "text")
            {
                badge.SetText(std::string(value));
                return true;
            }
            if (property == "bg-color")
            {
                if (const auto color = ParseColor(value))
                    badge.SetBackgroundColor(*color);
                return true;
            }
            if (property == "text-color")
            {
                if (const auto color = ParseColor(value))
                {
                    auto style = badge.TextStyle();
                    style.color = *color;
                    badge.SetTextStyle(std::move(style));
                }
                return true;
            }
            if (property == "hide-when-empty")
            {
                badge.SetHideWhenEmpty(ParseBoolean(value));
                return true;
            }
            return false;
        });
    RegisterControl("separator", [] { return std::make_unique<basic::DuiSeparator>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& separator = static_cast<basic::DuiSeparator&>(control);
            const std::string property = Normalize(name);
            if (property == "orientation" || property == "layout")
            {
                separator.SetOrientation(Normalize(value) == "vertical"
                    ? basic::DuiSeparator::Orientation::Vertical : basic::DuiSeparator::Orientation::Horizontal);
                return true;
            }
            if (property == "color")
            {
                if (const auto color = ParseColor(value))
                    separator.SetColor(*color);
                return true;
            }
            if (property == "thickness")
            {
                separator.SetThickness(ParseInteger(value, 1));
                return true;
            }
            if (property == "inset")
            {
                separator.SetInset(ParseInteger(value, 0));
                return true;
            }
            if (property == "text" || property == "content")
            {
                separator.SetText(std::string(value));
                return true;
            }
            if (property == "align")
            {
                const std::string align = Normalize(value);
                separator.SetTextAlign(align == "left" ? basic::DuiSeparatorTextAlign::Left
                    : align == "right" ? basic::DuiSeparatorTextAlign::Right
                    : basic::DuiSeparatorTextAlign::Center);
                return true;
            }
            if (property == "dashed")
            {
                separator.SetDashed(ParseBoolean(value));
                return true;
            }
            if (property == "gap" || property == "size")
            {
                separator.SetTextGap(ParseInteger(value, 8));
                return true;
            }
            return false;
        });
    RegisterControl("slider", [] { return std::make_unique<input::DuiSlider>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& slider = static_cast<input::DuiSlider&>(control);
            const std::string property = Normalize(name);
            if (property == "min")
            {
                slider.SetRange(ParseInteger(value, slider.Minimum()), slider.Maximum());
                return true;
            }
            if (property == "max")
            {
                slider.SetRange(slider.Minimum(), ParseInteger(value, slider.Maximum()));
                return true;
            }
            if (property == "value")
            {
                slider.SetValue(ParseInteger(value, slider.Value()));
                return true;
            }
            if (property == "line-size")
            {
                slider.SetLineSize(ParseInteger(value, 1));
                return true;
            }
            if (property == "vertical")
            {
                slider.SetVertical(ParseBoolean(value));
                return true;
            }
            return false;
        });
    RegisterControl("switch", [] { return std::make_unique<input::DuiSwitch>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& toggle = static_cast<input::DuiSwitch&>(control);
            const std::string property = Normalize(name);
            if (property == "checked")
            {
                toggle.SetChecked(ParseBoolean(value));
                return true;
            }
            if (property == "on-color" || property == "off-color" || property == "knob-color")
            {
                if (const auto color = ParseColor(value))
                {
                    if (property == "on-color")
                        toggle.SetOnColor(*color);
                    else if (property == "off-color")
                        toggle.SetOffColor(*color);
                    else
                        toggle.SetKnobColor(*color);
                }
                return true;
            }
            return false;
        });
    RegisterControl("progress", [] { return std::make_unique<feedback::DuiProgressBar>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& progress = static_cast<feedback::DuiProgressBar&>(control);
            const std::string property = Normalize(name);
            if (property == "min")
            {
                progress.SetRange(ParseInteger(value, progress.Minimum()), progress.Maximum());
                return true;
            }
            if (property == "max")
            {
                progress.SetRange(progress.Minimum(), ParseInteger(value, progress.Maximum()));
                return true;
            }
            if (property == "value")
            {
                progress.SetValue(ParseInteger(value, progress.Value()));
                return true;
            }
            if (property == "vertical")
            {
                progress.SetVertical(ParseBoolean(value));
                return true;
            }
            if (property == "marquee")
            {
                progress.SetMarquee(ParseBoolean(value));
                return true;
            }
            if (property == "bg-color" || property == "fill-color")
            {
                if (const auto color = ParseColor(value))
                {
                    if (property == "bg-color")
                        progress.SetBackgroundColor(*color);
                    else
                        progress.SetFillColor(*color);
                }
                return true;
            }
            return false;
        });
    RegisterControl("skeleton", [] { return std::make_unique<feedback::DuiSkeleton>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& skeleton = static_cast<feedback::DuiSkeleton&>(control);
            const std::string property = Normalize(name);
            if (property == "lines" || property == "line-count")
                skeleton.SetLineCount(ParseInteger(value, skeleton.LineCount()));
            else if (property == "line-height")
                skeleton.SetLineHeight(ParseInteger(value, skeleton.LineHeight()));
            else if (property == "gap")
                skeleton.SetGap(ParseInteger(value, skeleton.Gap()));
            else if (property == "corner-radius")
                skeleton.SetCornerRadius(ParseInteger(value, skeleton.CornerRadius()));
            else if (property == "line-width-percent")
                skeleton.SetLineWidthPercent(ParseInteger(value, skeleton.LineWidthPercent()));
            else
                return false;
            return true;
        });
    RegisterControl("status-bar", [] { return std::make_unique<basic::DuiStatusBar>(); });
    RegisterControl("breadcrumb", [] { return std::make_unique<basic::DuiBreadcrumb>(); });
    RegisterControl("segmented", [] { return std::make_unique<basic::DuiSegmentedControl>(); });
    RegisterControl("segmentedcontrol", [] { return std::make_unique<basic::DuiSegmentedControl>(); });
    RegisterControl("pagination", [] { return std::make_unique<list::DuiPagination>(); },
        [](core::Control& control, std::string_view name, std::string_view value)
        {
            auto& pagination = static_cast<list::DuiPagination&>(control);
            const std::string property = Normalize(name);
            if (property == "pages" || property == "page-count")
                pagination.SetPageCount(ParseInteger(value, pagination.PageCount()));
            else if (property == "page" || property == "current-page")
                pagination.SetCurrentPage(ParseInteger(value, 1) - 1);
            else if (property == "max-visible-pages")
                pagination.SetMaxVisiblePages(ParseInteger(value, pagination.MaxVisiblePages()));
            else
                return false;
            return true;
        });
}

DuiXmlBuilder::~DuiXmlBuilder() = default;
DuiXmlBuilder::DuiXmlBuilder(DuiXmlBuilder&&) noexcept = default;
DuiXmlBuilder& DuiXmlBuilder::operator=(DuiXmlBuilder&&) noexcept = default;

void DuiXmlBuilder::RegisterControl(std::string element, ControlCreator creator, PropertyApplier applier)
{
    element = Normalize(element);
    if (element.empty() || !creator)
        return;
    builder_->controls.insert_or_assign(std::move(element), Impl::Entry{std::move(creator), std::move(applier)});
}

void DuiXmlBuilder::SetResourceResolver(ResourceResolver resolver)
{
    builder_->resourceResolver = std::move(resolver);
}

void DuiXmlBuilder::SetImageResolver(ImageResolver resolver)
{
    builder_->imageResolver = std::move(resolver);
}

std::string DuiXmlBuilder::ResolveResourcePath(std::string_view path) const
{
    return builder_->resourceResolver ? builder_->resourceResolver(path) : std::string(path);
}

DuiXmlControlBuildResult DuiXmlBuilder::BuildWithResult(std::string_view xml) const
{
    DuiXmlControlBuildResult output;
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
    if (!result)
    {
        const DuiXmlErrorCode code = result.status == pugi::status_no_document_element
            ? DuiXmlErrorCode::MissingRootElement : DuiXmlErrorCode::ParseError;
        output.error = MakeXmlError(code, result.description(), xml, result.offset);
        return output;
    }
    if (!document.document_element())
    {
        output.error = MakeXmlError(DuiXmlErrorCode::MissingRootElement,
            "XML document does not contain a root element", xml, 0);
        return output;
    }

    std::optional<DuiXmlError> buildError;
    const auto buildNode = [this, xml, &buildError](const auto& self, pugi::xml_node node)
        -> std::unique_ptr<core::Control>
    {
        const std::string element = Normalize(node.name());
        const auto entry = builder_->controls.find(element);
        if (entry == builder_->controls.end())
        {
            buildError = MakeXmlError(DuiXmlErrorCode::UnknownElement,
                "Unknown XML element: " + element, xml, NodeSourceOffset(node), element);
            return {};
        }
        std::unique_ptr<core::Control> control = entry->second.creator();
        if (!control)
        {
            buildError = MakeXmlError(DuiXmlErrorCode::ControlCreationFailed,
                "Control creator returned no control for element: " + element,
                xml, NodeSourceOffset(node), element);
            return {};
        }
        if (auto* grid = dynamic_cast<layout::DuiGrid*>(control.get()))
        {
            grid->SetGrid(ParseInteger(node.attribute("rows").value(), 1),
                ParseInteger(node.attribute("columns").value(), 1));
        }
        if (auto* splitter = dynamic_cast<layout::DuiSplitter*>(control.get()))
        {
            splitter->SetMinSizes(ParseInteger(node.attribute("min-size-0").value(), 40),
                ParseInteger(node.attribute("min-size-1").value(), 40));
        }
        if (auto* nativeHost = dynamic_cast<window::DuiNativeHost*>(control.get()))
        {
            int width = ParseInteger(node.attribute("preferred-width").value(), 0);
            if (width <= 0)
                width = ParseInteger(node.attribute("preferredWidth").value(), 0);
            if (width <= 0)
                width = ParseInteger(node.attribute("width").value(), 0);
            int height = ParseInteger(node.attribute("preferred-height").value(), 0);
            if (height <= 0)
                height = ParseInteger(node.attribute("preferredHeight").value(), 0);
            if (height <= 0)
                height = ParseInteger(node.attribute("height").value(), 0);
            const core::Size current = nativeHost->PreferredSize();
            if (width > 0 || height > 0)
                nativeHost->SetPreferredSize({width > 0 ? width : current.width, height > 0 ? height : current.height});
            if (node.attribute("placeholder"))
                nativeHost->SetShowPlaceholder(ParseBoolean(node.attribute("placeholder").value()));
        }
        if (auto* dateTime = dynamic_cast<input::DuiDateTimePicker*>(control.get()))
        {
            const std::string mode = Normalize(node.attribute("mode").value());
            dateTime->SetMode(mode == "time" ? input::DuiDateTimePicker::Mode::Time
                : mode == "datetime" || mode == "date-time" ? input::DuiDateTimePicker::Mode::DateTime
                : input::DuiDateTimePicker::Mode::Date);
            if (node.attribute("format"))
                dateTime->SetFormat(node.attribute("format").value());
            if (node.attribute("value"))
            {
                core::DateTime value;
                const std::string text = node.attribute("value").value();
                const bool parsed = dateTime->GetMode() == input::DuiDateTimePicker::Mode::Time
                    ? input::DuiDateTimePicker::TryParseHms(text, value)
                    : dateTime->GetMode() == input::DuiDateTimePicker::Mode::DateTime
                        ? input::DuiDateTimePicker::TryParseYmdHms(text, value)
                            || input::DuiDateTimePicker::TryParseYmd(text, value)
                        : input::DuiDateTimePicker::TryParseYmd(text, value);
                if (parsed)
                    dateTime->SetDate(value);
            }
        }
        if (auto* hotKey = dynamic_cast<input::DuiHotKey*>(control.get()))
        {
            const int key = ParseInteger(node.attribute("vk").value(), 0);
            const int modifiers = node.attribute("mods")
                ? ParseInteger(node.attribute("mods").value(), 0)
                : ParseInteger(node.attribute("modifiers").value(), 0);
            if (key > 0)
                hotKey->SetHotKey(static_cast<unsigned int>(key), static_cast<unsigned int>(modifiers));
        }
        if (auto* dataGrid = dynamic_cast<list::DuiDataGrid*>(control.get()))
        {
            const auto checkboxes = node.attribute("show-checkboxes") ? node.attribute("show-checkboxes")
                : node.attribute("checkboxes");
            if (checkboxes)
                dataGrid->SetCheckboxesVisible(ParseBoolean(checkboxes.value()));
            const auto multiSelect = node.attribute("multi-select") ? node.attribute("multi-select")
                : node.attribute("multiselect");
            if (multiSelect)
                dataGrid->SetMultiSelect(ParseBoolean(multiSelect.value()));
        }
        if (auto* pagination = dynamic_cast<list::DuiPagination*>(control.get()))
        {
            const auto pages = node.attribute("pages") ? node.attribute("pages") : node.attribute("page-count");
            if (pages)
                pagination->SetPageCount(ParseInteger(pages.value(), 0));
            const auto maxVisible = node.attribute("max-visible-pages");
            if (maxVisible)
                pagination->SetMaxVisiblePages(ParseInteger(maxVisible.value(), pagination->MaxVisiblePages()));
            const auto page = node.attribute("page") ? node.attribute("page") : node.attribute("current-page");
            if (page)
                pagination->SetCurrentPage(ParseInteger(page.value(), 1) - 1);
        }
        for (const pugi::xml_attribute attribute : node.attributes())
        {
            const std::string_view name(attribute.name());
            const std::string_view value(attribute.value());
            const bool applied = entry->second.applier && entry->second.applier(*control, name, value);
            if (!applied)
                ApplyCommonProperty(*control, name, value);
        }
        if (auto* image = dynamic_cast<media::DuiImage*>(control.get()); image && builder_->imageResolver)
        {
            const pugi::xml_attribute source = node.attribute("src") ? node.attribute("src") : node.attribute("path");
            if (source)
                image->SetImage(builder_->imageResolver(ResolveResourcePath(source.value())));
        }
        if (auto* breadcrumb = dynamic_cast<basic::DuiBreadcrumb*>(control.get()))
        {
            breadcrumb->SetItems(SplitText(node.attribute("path").value(), "/>"));
        }
        std::vector<std::unique_ptr<core::Control>> expanderChildren;
        int dockPaneSequence{}; // 停靠管理器子元素缺省 id 的流水号
        for (const pugi::xml_node childNode : node.children())
        {
            if (childNode.type() != pugi::node_element)
                continue;
            if (const auto* scrollView = dynamic_cast<const layout::DuiScrollView*>(control.get());
                scrollView != nullptr && scrollView->Content() != nullptr)
            {
                continue;
            }
            if (auto* statusBar = dynamic_cast<basic::DuiStatusBar*>(control.get()))
            {
                if (Normalize(childNode.name()) == "pane")
                {
                    const bool spring = ParseBoolean(childNode.attribute("spring").value());
                    const int width = ParseInteger(childNode.attribute("width").value(), spring ? 0 : 80);
                    const int index = statusBar->AddPane(width > 0 ? width : 80, spring);
                    statusBar->SetPaneText(index, childNode.attribute("text").value());
                }
                continue;
            }
            if (auto* breadcrumb = dynamic_cast<basic::DuiBreadcrumb*>(control.get()))
            {
                const std::string childName = Normalize(childNode.name());
                if (childName == "item" || childName == "segment")
                    breadcrumb->AddItem(childNode.attribute("text").value());
                continue;
            }
            if (auto* segmented = dynamic_cast<basic::DuiSegmentedControl*>(control.get()))
            {
                const std::string childName = Normalize(childNode.name());
                if (childName == "item" || childName == "segment")
                    segmented->AddSegment(childNode.attribute("text").value());
                continue;
            }
            if (auto* comboBox = dynamic_cast<input::DuiComboBox*>(control.get()))
            {
                const std::string childName = Normalize(childNode.name());
                if (childName == "item" || childName == "option")
                {
                    const int index = comboBox->AddItem(childNode.attribute("text").value());
                    if (const auto icon = childNode.attribute("icon"); icon && builder_->imageResolver)
                        comboBox->SetItemIcon(index, builder_->imageResolver(ResolveResourcePath(icon.value())));
                }
                continue;
            }
            if (auto* listBox = dynamic_cast<list::DuiListBox*>(control.get()))
            {
                const std::string childName = Normalize(childNode.name());
                if (childName == "item" || childName == "option")
                {
                    const int index = listBox->AddItem(childNode.attribute("text").value(),
                        static_cast<std::uintptr_t>(ParseInteger(childNode.attribute("value").value(), 0)));
                    if (const auto icon = childNode.attribute("icon"); icon && builder_->imageResolver)
                        listBox->SetIconAt(index, builder_->imageResolver(ResolveResourcePath(icon.value())));
                    if (ParseBoolean(childNode.attribute("checked").value()))
                        listBox->SetChecked(index, true, false);
                }
                continue;
            }
            if (auto* dataGrid = dynamic_cast<list::DuiDataGrid*>(control.get()))
            {
                if (Normalize(childNode.name()) == "column")
                {
                    const std::string alignment = Normalize(childNode.attribute("align").value());
                    const render::DuiTextAlignment textAlignment = alignment == "center" ? render::DuiTextAlignment::Center
                        : alignment == "right" ? render::DuiTextAlignment::End : render::DuiTextAlignment::Start;
                    dataGrid->AddColumn({childNode.attribute("title").value(),
                        ParseInteger(childNode.attribute("width").value(), 120),
                        ParseInteger(childNode.attribute("min-width").value(), 40), textAlignment,
                        !childNode.attribute("sortable") || ParseBoolean(childNode.attribute("sortable").value())});
                }
                continue;
            }
            if (auto* tree = dynamic_cast<list::DuiTreeView*>(control.get()))
            {
                if (Normalize(childNode.name()) == "column")
                {
                    const std::string alignment = Normalize(childNode.attribute("align").value());
                    const render::DuiTextAlignment textAlignment = alignment == "center" ? render::DuiTextAlignment::Center
                        : alignment == "right" ? render::DuiTextAlignment::End : render::DuiTextAlignment::Start;
                    tree->AddColumn({childNode.attribute("title").value(),
                        ParseInteger(childNode.attribute("width").value(), 120),
                        ParseInteger(childNode.attribute("min-width").value(), 40), textAlignment,
                        !childNode.attribute("sortable") || ParseBoolean(childNode.attribute("sortable").value()),
                        !childNode.attribute("editable") || ParseBoolean(childNode.attribute("editable").value())});
                }
                continue;
            }
            if (auto* menuBar = dynamic_cast<list::DuiMenuBar*>(control.get()))
            {
                if (Normalize(childNode.name()) == "menu-item")
                    menuBar->AddItem(static_cast<std::uint32_t>(ParseInteger(childNode.attribute("id").value(), 0)),
                        childNode.attribute("text").value());
                continue;
            }
            if (auto* toolBar = dynamic_cast<basic::DuiToolBar*>(control.get()))
            {
                const std::string childName = Normalize(childNode.name());
                if (childName == "button" || childName == "item")
                    toolBar->AddButton(childNode.attribute("text").value(),
                        static_cast<std::uint32_t>(ParseInteger(childNode.attribute("id").value(), 0)));
                else if (childName == "separator")
                    toolBar->AddSeparator();
                else if (childName == "stretch" || childName == "spacer")
                    toolBar->AddStretch();
                continue;
            }
            std::unique_ptr<core::Control> child = self(self, childNode);
            if (buildError)
                return {};
            if (!child)
                continue;
            if (auto* stack = dynamic_cast<layout::DuiStack*>(control.get()))
            {
                layout::DuiStackItem item;
                item.weight = ParseInteger(childNode.attribute("weight").value(), 1);
                item.margin = ParseThickness(childNode.attribute("margin").value());
                const bool horizontal = stack->GetOrientation() == layout::DuiStackOrientation::Horizontal;
                item.mainLength = ParseInteger(childNode.attribute(horizontal ? "fixed-width" : "fixed-height").value(), -1);
                stack->AddChild(std::move(child), item);
            }
            else if (auto* dock = dynamic_cast<layout::DuiDock*>(control.get()))
            {
                const layout::DuiDockSide side = ParseDockSide(childNode.attribute("side").value());
                int size = ParseInteger(childNode.attribute("size").value(), 0);
                if (size <= 0)
                {
                    const char* fixedAttribute = side == layout::DuiDockSide::Top || side == layout::DuiDockSide::Bottom
                        ? "fixed-height" : "fixed-width";
                    size = ParseInteger(childNode.attribute(fixedAttribute).value(), 0);
                }
                dock->AddDocked(std::move(child), side, size);
            }
            else if (auto* flow = dynamic_cast<layout::DuiFlow*>(control.get()))
            {
                layout::DuiFlowItem item;
                item.desiredSize.width = ParseInteger(childNode.attribute("fixed-width").value(), 0);
                item.desiredSize.height = ParseInteger(childNode.attribute("fixed-height").value(), 0);
                item.margin = ParseFlowThickness(childNode.attribute("margin").value());
                flow->AddChild(std::move(child), item);
            }
            else if (auto* splitter = dynamic_cast<layout::DuiSplitter*>(control.get()))
            {
                if (splitter->GetPane(0) == nullptr)
                    splitter->SetPane(0, std::move(child));
                else if (splitter->GetPane(1) == nullptr)
                    splitter->SetPane(1, std::move(child));
            }
            else if (auto* dockManager = dynamic_cast<docking::DuiDockManager*>(control.get()))
            {
                // 停靠管理器：每个子元素成为一个窗格，id / title / slot 取自元素属性
                std::string id = childNode.attribute("id").value();
                if (id.empty())
                    id = "pane" + std::to_string(++dockPaneSequence);
                const std::string title = childNode.attribute("title").value();
                const docking::DuiDockSlot slot = ParseDockSlot(childNode.attribute("slot").value());                (void)dockManager->AddPane(id, title.empty() ? id : title, std::move(child), slot);
            }
            else if (auto* scrollView = dynamic_cast<layout::DuiScrollView*>(control.get()))
            {
                scrollView->SetContent(std::move(child));
            }
            else if (dynamic_cast<basic::DuiExpander*>(control.get()) != nullptr)
            {
                expanderChildren.push_back(std::move(child));
            }
            else if (auto* layout = dynamic_cast<layout::DuiLayout*>(control.get()))
            {
                layout::DuiLayoutHint hint;
                hint.weight = ParseInteger(childNode.attribute("weight").value(), 1);
                const layout::DuiThickness margin = ParseThickness(childNode.attribute("margin").value());
                hint.Margin(margin.left, margin.top, margin.right, margin.bottom);
                const bool horizontal = dynamic_cast<layout::DuiHBox*>(layout) != nullptr;
                const int fixedMain = ParseInteger(childNode.attribute(horizontal ? "fixed-width" : "fixed-height").value(), -1);
                if (fixedMain >= 0)
                    hint.Fixed(fixedMain);
                else
                    hint.Flexible(hint.weight);
                core::Control* rawChild = child.get();
                layout->AddChild(std::move(child), hint);
                if (auto* grid = dynamic_cast<layout::DuiGrid*>(layout))
                {
                    grid->SetCell(rawChild, ParseInteger(childNode.attribute("row").value(), 0),
                        ParseInteger(childNode.attribute("column").value(), 0),
                        ParseInteger(childNode.attribute("row-span").value(), 1),
                        ParseInteger(childNode.attribute("column-span").value(), 1));
                }
            }
            else if (auto* groupBox = dynamic_cast<basic::DuiGroupBox*>(control.get()))
            {
                if (groupBox->Content() == nullptr)
                    groupBox->SetContent(std::move(child));
            }
            else if (auto* card = dynamic_cast<basic::DuiCard*>(control.get()))
            {
                if (card->Content() == nullptr)
                    card->SetContent(std::move(child));
            }
            else
                control->AddChild(std::move(child));
        }
        if (auto* expander = dynamic_cast<basic::DuiExpander*>(control.get()))
        {
            if (expanderChildren.size() == 1)
                expander->SetContent(std::move(expanderChildren.front()));
            else if (!expanderChildren.empty())
            {
                auto content = std::make_unique<layout::DuiVBox>();
                for (auto& child : expanderChildren)
                    content->AddChild(std::move(child), {});
                expander->SetContent(std::move(content));
            }
        }
        if (auto* segmented = dynamic_cast<basic::DuiSegmentedControl*>(control.get()))
        {
            if (segmented->SegmentCount() == 0)
            {
                for (std::string item : SplitText(node.attribute("items").value(), ","))
                    segmented->AddSegment(std::move(item));
            }
            const pugi::xml_attribute selected = node.attribute("current") ? node.attribute("current") : node.attribute("index");
            if (selected && segmented->SegmentCount() > 0)
                segmented->SetSelectedIndex(ParseInteger(selected.value(), 0));
        }
        if (auto* comboBox = dynamic_cast<input::DuiComboBox*>(control.get()))
        {
            const pugi::xml_attribute selected = node.attribute("selected-index") ? node.attribute("selected-index")
                : node.attribute("selected");
            if (selected)
                comboBox->SetSelectedIndex(ParseInteger(selected.value(), -1), false);
        }
        if (auto* listBox = dynamic_cast<list::DuiListBox*>(control.get()))
        {
            const pugi::xml_attribute selected = node.attribute("selected-index") ? node.attribute("selected-index")
                : node.attribute("selected");
            if (selected)
                listBox->SetSelectedIndex(ParseInteger(selected.value(), -1), false);
        }
        if (auto* tree = dynamic_cast<list::DuiTreeView*>(control.get()))
        {
            if (node.attribute("multi-select"))
                tree->SetMultiSelect(ParseBoolean(node.attribute("multi-select").value()));
            if (node.attribute("node-checks"))
                tree->SetNodeChecksVisible(ParseBoolean(node.attribute("node-checks").value()));
            if (node.attribute("frozen-cols"))
                tree->SetFrozenColumns(ParseInteger(node.attribute("frozen-cols").value(), 0));
            if (node.attribute("frozen-rows"))
                tree->SetFrozenRows(ParseInteger(node.attribute("frozen-rows").value(), 0));
        }
        return control;
    };
    output.control = buildNode(buildNode, document.document_element());
    if (buildError)
    {
        output.control.reset();
        output.error = std::move(buildError);
    }
    return output;
}

std::unique_ptr<core::Control> DuiXmlBuilder::Build(std::string_view xml) const
{
    DuiXmlControlBuildResult result = BuildWithResult(xml);
    return std::move(result.control);
}

DuiFrameXmlBuildResult DuiXmlBuilder::BuildFrameWithResult(std::string_view xml) const
{
    DuiFrameXmlBuildResult output;
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(xml.data(), xml.size(), pugi::parse_default,
        pugi::encoding_utf8);
    if (!result)
    {
        const DuiXmlErrorCode code = result.status == pugi::status_no_document_element
            ? DuiXmlErrorCode::MissingRootElement : DuiXmlErrorCode::ParseError;
        output.error = MakeXmlError(code, result.description(), xml, result.offset);
        return output;
    }
    const pugi::xml_node frame = document.document_element();
    if (!frame)
    {
        output.error = MakeXmlError(DuiXmlErrorCode::MissingRootElement,
            "XML document does not contain a root element", xml, 0);
        return output;
    }
    if (Normalize(frame.name()) != "frame-window")
    {
        output.error = MakeXmlError(DuiXmlErrorCode::InvalidFrameRoot,
            "Frame XML root element must be frame-window", xml, NodeSourceOffset(frame), frame.name());
        return output;
    }

    pugi::xml_node contentNode;
    for (const pugi::xml_node child : frame.children())
    {
        if (child.type() != pugi::node_element)
            continue;
        if (contentNode)
        {
            output.error = MakeXmlError(DuiXmlErrorCode::InvalidFrameContent,
                "Frame XML must contain exactly one content element", xml,
                NodeSourceOffset(child), frame.name());
            return output;
        }
        contentNode = child;
    }
    if (!contentNode)
    {
        output.error = MakeXmlError(DuiXmlErrorCode::InvalidFrameContent,
            "Frame XML must contain exactly one content element", xml,
            NodeSourceOffset(frame), frame.name());
        return output;
    }

    DuiFrameXmlDocument& resultDocument = output.document;
    ui::DuiFrameOptions& options = resultDocument.options;
    const auto setInvalidAttribute = [&output, xml, frame](std::string_view attribute, std::string message)
    {
        output.error = MakeXmlError(DuiXmlErrorCode::InvalidAttribute, std::move(message), xml,
            AttributeSourceOffset(xml, frame, attribute), frame.name(), std::string(attribute));
    };
    const auto readInteger = [&frame, &setInvalidAttribute](const char* name, int minimum, int& destination)
    {
        const pugi::xml_attribute attribute = frame.attribute(name);
        if (!attribute)
            return true;
        const auto parsed = TryParseInteger(attribute.value());
        if (!parsed)
        {
            setInvalidAttribute(name, "Invalid integer value for XML attribute: " + std::string(name));
            return false;
        }
        destination = (std::max)(minimum, *parsed);
        return true;
    };
    const auto readBoolean = [&frame, &setInvalidAttribute](const char* name, bool& destination)
    {
        const pugi::xml_attribute attribute = frame.attribute(name);
        if (!attribute)
            return true;
        const auto parsed = TryParseBoolean(attribute.value());
        if (!parsed)
        {
            setInvalidAttribute(name, "Invalid boolean value for XML attribute: " + std::string(name));
            return false;
        }
        destination = *parsed;
        return true;
    };
    const auto readColor = [&frame, &setInvalidAttribute](const char* name, core::Color& destination)
    {
        const pugi::xml_attribute attribute = frame.attribute(name);
        if (!attribute)
            return true;
        const auto parsed = ParseColor(attribute.value());
        if (!parsed)
        {
            setInvalidAttribute(name, "Invalid color value for XML attribute: " + std::string(name));
            return false;
        }
        destination = *parsed;
        return true;
    };

    if (const pugi::xml_attribute title = frame.attribute("title"))
        options.title = title.value();
    if (!readInteger("title-bar-height", 0, options.titleBarHeight)
        || !readBoolean("title-bar-transparent", options.titleBarTransparent)
        || !readColor("title-text-color", options.titleTextColor)
        || !readColor("caption-glyph-color", options.captionGlyphColor)
        || !readBoolean("has-min-button", options.showMinimize)
        || !readBoolean("has-max-button", options.showMaximize)
        || !readBoolean("has-close-button", options.showClose)
        || !readInteger("min-w", 1, options.minimumSize.width)
        || !readInteger("min-h", 1, options.minimumSize.height)
        || !readInteger("max-w", 0, options.maximumSize.width)
        || !readInteger("max-h", 0, options.maximumSize.height)
        || !readBoolean("resizable", options.resizable)
        || !readInteger("border-px", 0, options.resizeBorder))
    {
        return output;
    }
    if (const pugi::xml_attribute insets = frame.attribute("bg-src-insets"))
    {
        const layout::DuiThickness parsed = ParseThickness(insets.value());
        options.backgroundSourceInsets = {parsed.left, parsed.top, parsed.right, parsed.bottom};
    }
    if (const pugi::xml_attribute insets = frame.attribute("bg-dst-insets"))
    {
        const layout::DuiThickness parsed = ParseThickness(insets.value());
        options.backgroundDestinationInsets = {parsed.left, parsed.top, parsed.right, parsed.bottom};
    }
    if (const pugi::xml_attribute image = frame.attribute("bg-image"); image && builder_->imageResolver)
        options.backgroundImage = builder_->imageResolver(ResolveResourcePath(image.value()));

    std::ostringstream contentXml;
    contentNode.print(contentXml, "", pugi::format_raw, pugi::encoding_utf8);
    const std::string contentText = contentXml.str();
    DuiXmlControlBuildResult contentResult = BuildWithResult(contentText);
    if (!contentResult)
    {
        if (contentResult.error)
        {
            const std::size_t serializedRoot = contentText.find(contentNode.name());
            const std::ptrdiff_t baseOffset = NodeSourceOffset(contentNode)
                - static_cast<std::ptrdiff_t>(serializedRoot == std::string::npos ? 0 : serializedRoot);
            DuiXmlError& error = *contentResult.error;
            output.error = MakeXmlError(error.code, std::move(error.message), xml,
                baseOffset + static_cast<std::ptrdiff_t>(error.offset),
                std::move(error.element), std::move(error.attribute));
        }
        return output;
    }
    resultDocument.content = std::move(contentResult.control);
    return output;
}

std::optional<DuiFrameXmlDocument> DuiXmlBuilder::BuildFrame(std::string_view xml) const
{
    DuiFrameXmlBuildResult result = BuildFrameWithResult(xml);
    if (!result)
        return {};
    return std::move(result.document);
}

} // namespace ysDui::controls
