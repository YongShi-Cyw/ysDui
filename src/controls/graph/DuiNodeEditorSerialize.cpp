#include "ysDui/controls/graph/DuiNodeEditor.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "DuiNodeEditorInternal.hpp"
#include "DuiUtf8Path.hpp"
#include "ysDui/render/DuiXmlDocument.hpp"

namespace ysDui::controls::graph {
namespace {

constexpr std::string_view SchemaVersion = "1";

/** 把 XML 属性值中的敏感字符转义，保证往返可解析。 */
std::string EscapeXml(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char character : text)
    {
        switch (character)
        {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        default: result.push_back(character); break;
        }
    }
    return result;
}

/** 还原 XML 属性值中的转义字符，保证与 EscapeXml 对称。 */
std::string UnescapeXml(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] != '&')
        {
            result.push_back(text[index]);
            continue;
        }
        const std::size_t end = text.find(';', index);
        if (end == std::string_view::npos)
        {
            result.push_back(text[index]);
            continue;
        }
        const std::string_view entity = text.substr(index + 1, end - index - 1);
        if (entity == "amp") result.push_back('&');
        else if (entity == "lt") result.push_back('<');
        else if (entity == "gt") result.push_back('>');
        else if (entity == "quot") result.push_back('"');
        else if (entity == "apos") result.push_back('\'');
        else
        {
            // 未知实体原样保留，避免静默丢字符
            result.append(text.substr(index, end - index + 1));
        }
        index = end;
    }
    return result;
}

/** 解析无符号整数；失败返回 nullopt（不抛异常，与库内风格一致）。 */
std::optional<std::uint64_t> ParseUnsigned(std::string_view text)
{
    if (text.empty())
        return std::nullopt;
    std::uint64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}

/** 解析有符号整数；失败返回 nullopt。 */
std::optional<int> ParseSigned(std::string_view text)
{
    if (text.empty())
        return std::nullopt;
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}

/** 解析浮点数；失败返回 nullopt。 */
std::optional<double> ParseDouble(std::string_view text)
{
    if (text.empty())
        return std::nullopt;
    double value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}

/** 颜色序列化为 "r,g,b,a"（十进制，便于手写与 diff）。 */
std::string FormatColor(core::Color color)
{
    return std::to_string(static_cast<unsigned>(color.red)) + ","
        + std::to_string(static_cast<unsigned>(color.green)) + ","
        + std::to_string(static_cast<unsigned>(color.blue)) + ","
        + std::to_string(static_cast<unsigned>(color.alpha));
}

/** 解析 "r,g,b,a"；分量缺失、多余尾巴或越界返回 nullopt。 */
std::optional<core::Color> ParseColor(std::string_view text)
{
    unsigned char channels[4]{};
    std::size_t begin{};
    for (int index = 0; index < 4; ++index)
    {
        const std::size_t comma = text.find(',', begin);
        const std::size_t end = index < 3 ? comma : text.size();
        if (index < 3 && comma == std::string_view::npos)
            return std::nullopt;
        const std::optional<std::uint64_t> value = ParseUnsigned(text.substr(begin, end - begin));
        if (!value || *value > 255)
            return std::nullopt;
        channels[index] = static_cast<unsigned char>(*value);
        begin = end + 1;
    }
    return core::Color{channels[0], channels[1], channels[2], channels[3]};
}

/** 极简 XML 元素扫描：本控件只需读取自己写出的固定结构，不引入通用解析依赖。 */
struct Element
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
};

/** @return 文本中的全部元素（起始标签），属性已做反转义。 */
std::vector<Element> ScanElements(std::string_view xml)
{
    std::vector<Element> result;
    std::size_t cursor{};
    while (true)
    {
        const std::size_t open = xml.find('<', cursor);
        if (open == std::string_view::npos)
            break;
        const std::size_t close = xml.find('>', open);
        if (close == std::string_view::npos)
            break;
        std::string_view body = xml.substr(open + 1, close - open - 1);
        cursor = close + 1;
        if (body.empty() || body.front() == '/' || body.front() == '?' || body.front() == '!')
            continue;
        if (body.back() == '/')
            body.remove_suffix(1);
        Element element;
        std::size_t position{};
        while (position < body.size() && body[position] != ' ' && body[position] != '\t'
               && body[position] != '\n' && body[position] != '\r')
        {
            element.name.push_back(body[position]);
            ++position;
        }
        // 属性：name="value"（值内不含已转义的引号）
        while (true)
        {
            const std::size_t nameStart = body.find_first_not_of(" \t\r\n", position);
            if (nameStart == std::string_view::npos)
                break;
            const std::size_t equals = body.find('=', nameStart);
            if (equals == std::string_view::npos)
                break;
            const std::size_t quoteOpen = body.find('"', equals);
            if (quoteOpen == std::string_view::npos)
                break;
            const std::size_t quoteClose = body.find('"', quoteOpen + 1);
            if (quoteClose == std::string_view::npos)
                break;
            element.attributes.emplace_back(
                std::string(body.substr(nameStart, equals - nameStart)),
                UnescapeXml(body.substr(quoteOpen + 1, quoteClose - quoteOpen - 1)));
            position = quoteClose + 1;
        }
        result.push_back(std::move(element));
    }
    return result;
}

/** @return 指定元素的属性值；不存在返回 nullptr。 */
const std::string* Attribute(const Element& element, std::string_view name)
{
    for (const auto& entry : element.attributes)
    {
        if (entry.first == name)
            return &entry.second;
    }
    return nullptr;
}

} // namespace

std::string SerializeGraphXml(const DuiNodeGraph& graph)
{
    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    out << "<nodeGraph version=\"" << SchemaVersion << "\">\n";
    for (const auto& node : graph.Nodes())
    {
        out << "  <node id=\"" << node.id.value << "\""
            << " kind=\"" << (node.kind == DuiNodeKind::Group ? "group"
                : node.kind == DuiNodeKind::Comment ? "comment" : "node") << "\""
            << " title=\"" << EscapeXml(node.title) << "\""
            << " x=\"" << node.position.x << "\" y=\"" << node.position.y << "\""
            << " w=\"" << node.size.width << "\" h=\"" << node.size.height << "\"";
        if (node.group.Valid())
            out << " group=\"" << node.group.value << "\"";
        if (node.hasColor)
            out << " color=\"" << FormatColor(node.color) << "\"";
        out << "/>\n";
    }
    for (const auto& pin : graph.Pins())
    {
        out << "  <pin id=\"" << pin.id.value << "\""
            << " node=\"" << pin.node.value << "\""
            << " kind=\"" << (pin.kind == DuiPinKind::Input ? "input" : "output") << "\""
            << " multiple=\"" << (pin.multipleConnections ? 1 : 0) << "\""
            << " name=\"" << EscapeXml(pin.name) << "\"";
        if (!pin.typeId.empty())
            out << " type=\"" << EscapeXml(pin.typeId) << "\"";
        if (pin.hasColor)
            out << " color=\"" << FormatColor(pin.color) << "\"";
        out << "/>\n";
    }
    for (const auto& link : graph.Links())
    {
        out << "  <link id=\"" << link.id.value << "\""
            << " start=\"" << link.start.value << "\""
            << " end=\"" << link.end.value << "\"";
        if (link.hasColor)
            out << " color=\"" << FormatColor(link.color) << "\"";
        out << "/>\n";
    }
    out << "</nodeGraph>\n";
    return out.str();
}

bool DeserializeGraphXml(std::string_view xml, DuiNodeGraph& graph)
{
    const std::vector<Element> elements = ScanElements(xml);
    const bool hasRoot = std::any_of(elements.begin(), elements.end(),
                                     [](const Element& element) { return element.name == "nodeGraph"; });
    if (!hasRoot)
        return false;

    DuiNodeGraph rebuilt;
    for (const Element& element : elements)
    {
        if (element.name != "node")
            continue;
        const std::string* id = Attribute(element, "id");
        const std::string* x = Attribute(element, "x");
        const std::string* y = Attribute(element, "y");
        const std::optional<std::uint64_t> nodeId = id != nullptr ? ParseUnsigned(*id) : std::nullopt;
        const std::optional<int> positionX = x != nullptr ? ParseSigned(*x) : std::nullopt;
        const std::optional<int> positionY = y != nullptr ? ParseSigned(*y) : std::nullopt;
        if (!nodeId || !positionX || !positionY)
            return false;
        DuiGraphNode node;
        node.id = DuiNodeId{*nodeId};
        const std::string* kind = Attribute(element, "kind");
        if (kind != nullptr && *kind == "group")
            node.kind = DuiNodeKind::Group;
        else if (kind != nullptr && *kind == "comment")
            node.kind = DuiNodeKind::Comment;
        else
            node.kind = DuiNodeKind::Normal;
        const std::string* title = Attribute(element, "title");
        node.title = title != nullptr ? *title : std::string{};
        node.position = {*positionX, *positionY};
        if (const std::string* group = Attribute(element, "group"); group != nullptr)
        {
            const std::optional<std::uint64_t> groupId = ParseUnsigned(*group);
            if (!groupId)
                return false;
            node.group = DuiNodeId{*groupId};
        }
        if (node.kind == DuiNodeKind::Group || node.kind == DuiNodeKind::Comment)
        {
            const std::string* w = Attribute(element, "w");
            const std::string* h = Attribute(element, "h");
            const std::optional<int> width = w != nullptr ? ParseSigned(*w) : std::nullopt;
            const std::optional<int> height = h != nullptr ? ParseSigned(*h) : std::nullopt;
            if (!width || !height)
                return false;
            node.size = {*width, *height};
        }
        if (const std::string* color = Attribute(element, "color"); color != nullptr)
        {
            const std::optional<core::Color> parsed = ParseColor(*color);
            if (!parsed)
                return false;
            node.color = *parsed;
            node.hasColor = true;
        }
        if (!rebuilt.RestoreNode(node))
            return false;
    }
    for (const Element& element : elements)
    {
        if (element.name != "pin")
            continue;
        const std::string* id = Attribute(element, "id");
        const std::string* node = Attribute(element, "node");
        const std::optional<std::uint64_t> pinId = id != nullptr ? ParseUnsigned(*id) : std::nullopt;
        const std::optional<std::uint64_t> nodeId = node != nullptr ? ParseUnsigned(*node) : std::nullopt;
        if (!pinId || !nodeId)
            return false;
        DuiGraphPin pin;
        pin.id = DuiPinId{*pinId};
        pin.node = DuiNodeId{*nodeId};
        const std::string* kind = Attribute(element, "kind");
        pin.kind = kind != nullptr && *kind == "output" ? DuiPinKind::Output : DuiPinKind::Input;
        const std::string* name = Attribute(element, "name");
        pin.name = name != nullptr ? *name : std::string{};
        const std::string* multiple = Attribute(element, "multiple");
        pin.multipleConnections = multiple != nullptr && *multiple == "1";
        if (const std::string* type = Attribute(element, "type"); type != nullptr)
            pin.typeId = *type;
        if (const std::string* color = Attribute(element, "color"); color != nullptr)
        {
            const std::optional<core::Color> parsed = ParseColor(*color);
            if (!parsed)
                return false;
            pin.color = *parsed;
            pin.hasColor = true;
        }
        if (!rebuilt.RestorePin(pin))
            return false;
    }
    for (const Element& element : elements)
    {
        if (element.name != "link")
            continue;
        const std::string* id = Attribute(element, "id");
        const std::string* start = Attribute(element, "start");
        const std::string* end = Attribute(element, "end");
        const std::optional<std::uint64_t> linkId = id != nullptr ? ParseUnsigned(*id) : std::nullopt;
        const std::optional<std::uint64_t> startId = start != nullptr ? ParseUnsigned(*start) : std::nullopt;
        const std::optional<std::uint64_t> endId = end != nullptr ? ParseUnsigned(*end) : std::nullopt;
        if (!linkId || !startId || !endId)
            return false;
        DuiGraphLink link;
        link.id = DuiLinkId{*linkId};
        link.start = DuiPinId{*startId};
        link.end = DuiPinId{*endId};
        if (const std::string* color = Attribute(element, "color"); color != nullptr)
        {
            const std::optional<core::Color> parsed = ParseColor(*color);
            if (!parsed)
                return false;
            link.color = *parsed;
            link.hasColor = true;
        }
        if (!rebuilt.RestoreLink(link))
            return false;
    }
    graph = std::move(rebuilt);
    return true;
}

std::string DuiNodeEditor::ToXml() const
{
    std::string xml = SerializeGraphXml(editor_->graph);
    const std::string view = "  <view zoom=\"" + std::to_string(editor_->transform.scale)
        + "\" panX=\"" + std::to_string(editor_->transform.offset.x)
        + "\" panY=\"" + std::to_string(editor_->transform.offset.y) + "\"/>\n";
    const std::size_t insert = xml.find('>', xml.find("<nodeGraph"));
    if (insert != std::string::npos)
        xml.insert(insert + 2, view);
    return xml;
}

bool DuiNodeEditor::FromXml(std::string_view xml)
{
    DuiNodeGraph rebuilt;
    if (!DeserializeGraphXml(xml, rebuilt))
        return false;

    editor_->graph = std::move(rebuilt);
    editor_->ClearSelection();
    editor_->undoStack.clear();
    editor_->redoStack.clear();
    editor_->NotifyGraphChanged();
    const std::vector<Element> elements = ScanElements(xml);
    for (const Element& element : elements)
    {
        if (element.name != "view")
            continue;
        const std::string* zoom = Attribute(element, "zoom");
        const std::string* panX = Attribute(element, "panX");
        const std::string* panY = Attribute(element, "panY");
        const std::optional<double> scale = zoom != nullptr ? ParseDouble(*zoom) : std::nullopt;
        const std::optional<int> offsetX = panX != nullptr ? ParseSigned(*panX) : std::nullopt;
        const std::optional<int> offsetY = panY != nullptr ? ParseSigned(*panY) : std::nullopt;
        if (scale)
            SetZoom(*scale);
        if (offsetX && offsetY)
            SetPanOffset({*offsetX, *offsetY});
        break;
    }
    return true;
}

bool DuiNodeEditor::SaveToFile(const std::string& utf8Path) const
{
    const std::optional<std::filesystem::path> path = core::detail::PathFromUtf8(utf8Path);
    if (!path)
        return false;
    std::ofstream file(*path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    const std::string xml = ToXml();
    file.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    return file.good();
}

bool DuiNodeEditor::LoadFromFile(const std::string& utf8Path)
{
    const std::optional<std::filesystem::path> path = core::detail::PathFromUtf8(utf8Path);
    if (!path)
        return false;
    std::ifstream file(*path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    const std::streamsize size = file.tellg();
    if (size <= 0)
        return false;
    file.seekg(0, std::ios::beg);
    std::string xml(static_cast<std::size_t>(size), '\0');
    file.read(xml.data(), size);
    if (!file.good() && !file.eof())
        return false;
    return FromXml(xml);
}

} // namespace ysDui::controls::graph
