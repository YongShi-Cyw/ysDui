/**
 * 文件名：DuiRichDocumentJson.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现富文档 UTF-8 JSON 编解码和资源图像引用验证。
 */
#include "ysDui/controls/input/DuiRichDocumentJson.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace ysDui::controls::input {
namespace {

enum class JsonType
{
    Boolean,
    Number,
    String,
    Array,
    Object,
};

struct JsonValue final
{
    JsonType type{};
    bool boolean{};
    std::uint64_t number{};
    std::string string;
    std::vector<JsonValue> array;
    // 键值拆成平行数组：避免 vector<pair<string, JsonValue>> 在类型未完成时触发 Clang/libstdc++ 错误。
    std::vector<std::string> objectKeys;
    std::vector<JsonValue> objectValues;
};

class JsonReader final
{
public:
    explicit JsonReader(std::string_view source) : source_(source) {}

    [[nodiscard]] std::optional<JsonValue> Read()
    {
        std::optional<JsonValue> value = readValue();
        skipWhitespace();
        return value && position_ == source_.size() ? value : std::nullopt;
    }

private:
    void skipWhitespace()
    {
        while (position_ < source_.size() && (source_[position_] == ' ' || source_[position_] == '\t'
            || source_[position_] == '\r' || source_[position_] == '\n'))
            ++position_;
    }

    [[nodiscard]] bool consume(char expected)
    {
        skipWhitespace();
        if (position_ == source_.size() || source_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    [[nodiscard]] std::optional<JsonValue> readValue()
    {
        skipWhitespace();
        if (position_ == source_.size())
            return std::nullopt;
        switch (source_[position_])
        {
        case '{':
            return readObject();
        case '[':
            return readArray();
        case '"':
        {
            std::optional<std::string> string = readString();
            if (!string)
                return std::nullopt;
            JsonValue result{};
            result.type = JsonType::String;
            result.string = std::move(*string);
            return result;
        }
        case 't':
        {
            JsonValue result{};
            result.type = JsonType::Boolean;
            result.boolean = true;
            return consumeLiteral("true", std::move(result));
        }
        case 'f':
        {
            JsonValue result{};
            result.type = JsonType::Boolean;
            result.boolean = false;
            return consumeLiteral("false", std::move(result));
        }
        default:
            return readNumber();
        }
    }

    [[nodiscard]] std::optional<JsonValue> readObject()
    {
        if (!consume('{'))
            return std::nullopt;
        JsonValue result{};
        result.type = JsonType::Object;
        skipWhitespace();
        if (position_ < source_.size() && source_[position_] == '}')
        {
            ++position_;
            return result;
        }
        while (true)
        {
            std::optional<std::string> key = readString();
            if (!key || !consume(':'))
                return std::nullopt;
            std::optional<JsonValue> value = readValue();
            if (!value || findMember(result, *key) != nullptr)
                return std::nullopt;
            result.objectKeys.push_back(std::move(*key));
            result.objectValues.push_back(std::move(*value));
            skipWhitespace();
            if (position_ < source_.size() && source_[position_] == '}')
            {
                ++position_;
                return result;
            }
            if (!consume(','))
                return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<JsonValue> readArray()
    {
        if (!consume('['))
            return std::nullopt;
        JsonValue result{};
        result.type = JsonType::Array;
        skipWhitespace();
        if (position_ < source_.size() && source_[position_] == ']')
        {
            ++position_;
            return result;
        }
        while (true)
        {
            std::optional<JsonValue> value = readValue();
            if (!value)
                return std::nullopt;
            result.array.push_back(std::move(*value));
            skipWhitespace();
            if (position_ < source_.size() && source_[position_] == ']')
            {
                ++position_;
                return result;
            }
            if (!consume(','))
                return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<JsonValue> readNumber()
    {
        skipWhitespace();
        const std::size_t begin = position_;
        while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9')
            ++position_;
        if (begin == position_ || (position_ - begin > 1 && source_[begin] == '0'))
            return std::nullopt;
        std::uint64_t value{};
        const auto parsed = std::from_chars(source_.data() + begin, source_.data() + position_, value);
        if (parsed.ec != std::errc{} || parsed.ptr != source_.data() + position_)
            return std::nullopt;
        JsonValue result{};
        result.type = JsonType::Number;
        result.number = value;
        return result;
    }

    [[nodiscard]] std::optional<std::string> readString()
    {
        if (!consume('"'))
            return std::nullopt;
        std::string result;
        while (position_ < source_.size())
        {
            const unsigned char character = static_cast<unsigned char>(source_[position_++]);
            if (character == '"')
                return result;
            if (character < 0x20U)
                return std::nullopt;
            if (character != '\\')
            {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ == source_.size())
                return std::nullopt;
            const char escaped = source_[position_++];
            switch (escaped)
            {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u':
            {
                const std::optional<std::uint16_t> first = readHexCodeUnit();
                if (!first)
                    return std::nullopt;
                char32_t codePoint = *first;
                if (codePoint >= 0xD800U && codePoint <= 0xDBFFU)
                {
                    if (position_ + 2 > source_.size() || source_[position_] != '\\' || source_[position_ + 1] != 'u')
                        return std::nullopt;
                    position_ += 2;
                    const std::optional<std::uint16_t> second = readHexCodeUnit();
                    if (!second || *second < 0xDC00U || *second > 0xDFFFU)
                        return std::nullopt;
                    codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (*second - 0xDC00U);
                }
                else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU)
                    return std::nullopt;
                appendUtf8(result, codePoint);
                break;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint16_t> readHexCodeUnit()
    {
        if (position_ + 4 > source_.size())
            return std::nullopt;
        std::uint16_t result{};
        for (int index{}; index < 4; ++index)
        {
            const char character = source_[position_++];
            const int value = character >= '0' && character <= '9' ? character - '0'
                : character >= 'a' && character <= 'f' ? character - 'a' + 10
                : character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
            if (value < 0)
                return std::nullopt;
            result = static_cast<std::uint16_t>((result << 4U) | static_cast<std::uint16_t>(value));
        }
        return result;
    }

    [[nodiscard]] std::optional<JsonValue> consumeLiteral(std::string_view literal, JsonValue value)
    {
        if (source_.substr(position_, literal.size()) != literal)
            return std::nullopt;
        position_ += literal.size();
        return value;
    }

    static const JsonValue* findMember(const JsonValue& object, std::string_view key)
    {
        for (std::size_t index = 0; index < object.objectKeys.size(); ++index)
        {
            if (object.objectKeys[index] == key)
                return &object.objectValues[index];
        }
        return nullptr;
    }

    static void appendUtf8(std::string& target, char32_t codePoint)
    {
        if (codePoint <= 0x7FU)
            target.push_back(static_cast<char>(codePoint));
        else if (codePoint <= 0x7FFU)
        {
            target.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
            target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else if (codePoint <= 0xFFFFU)
        {
            target.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
            target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else
        {
            target.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
            target.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
            target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    std::string_view source_;
    std::size_t position_{};
};

const JsonValue* FindMember(const JsonValue& object, std::string_view key)
{
    if (object.type != JsonType::Object)
        return nullptr;
    for (std::size_t index = 0; index < object.objectKeys.size(); ++index)
    {
        if (object.objectKeys[index] == key)
            return &object.objectValues[index];
    }
    return nullptr;
}

bool IsType(const JsonValue* value, JsonType type)
{
    return value != nullptr && value->type == type;
}

void AppendUtf8(std::string& target, char32_t codePoint);

std::optional<std::string> DecodeUtf8(std::string_view value)
{
    std::string result;
    for (std::size_t index{}; index < value.size();)
    {
        const unsigned char first = static_cast<unsigned char>(value[index++]);
        char32_t codePoint{};
        int continuationCount{};
        if ((first & 0x80U) == 0)
            codePoint = first;
        else if ((first & 0xE0U) == 0xC0U)
        {
            codePoint = first & 0x1FU;
            continuationCount = 1;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            codePoint = first & 0x0FU;
            continuationCount = 2;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            codePoint = first & 0x07U;
            continuationCount = 3;
        }
        else
            return std::nullopt;
        if (index + static_cast<std::size_t>(continuationCount) > value.size())
            return std::nullopt;
        for (int count{}; count < continuationCount; ++count)
        {
            const unsigned char continuation = static_cast<unsigned char>(value[index++]);
            if ((continuation & 0xC0U) != 0x80U)
                return std::nullopt;
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }
        if ((continuationCount == 1 && codePoint < 0x80U) || (continuationCount == 2 && codePoint < 0x800U)
            || (continuationCount == 3 && codePoint < 0x10000U) || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            return std::nullopt;
        AppendUtf8(result, codePoint);
    }
    return result;
}

void AppendUtf8(std::string& target, char32_t codePoint)
{
    if (codePoint <= 0x7FU)
        target.push_back(static_cast<char>(codePoint));
    else if (codePoint <= 0x7FFU)
    {
        target.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
    else if (codePoint <= 0xFFFFU)
    {
        target.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
    else
    {
        target.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
}

void AppendEscaped(std::string& target, std::string_view value)
{
    target.push_back('"');
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"': target += "\\\""; break;
        case '\\': target += "\\\\"; break;
        case '\b': target += "\\b"; break;
        case '\f': target += "\\f"; break;
        case '\n': target += "\\n"; break;
        case '\r': target += "\\r"; break;
        case '\t': target += "\\t"; break;
        default:
            if (character < 0x20U)
            {
                constexpr char hex[] = "0123456789ABCDEF";
                target += "\\u00";
                target.push_back(hex[character >> 4U]);
                target.push_back(hex[character & 0x0FU]);
            }
            else
                target.push_back(static_cast<char>(character));
            break;
        }
    }
    target.push_back('"');
}

void AppendUtf16String(std::string& target, std::string_view value)
{
    AppendEscaped(target, value);
}

void AppendColor(std::string& target, core::Color color)
{
    target += '[';
    target += std::to_string(color.red);
    target += ',';
    target += std::to_string(color.green);
    target += ',';
    target += std::to_string(color.blue);
    target += ',';
    target += std::to_string(color.alpha);
    target += ']';
}

std::optional<core::Color> ReadColor(const JsonValue* value)
{
    if (!IsType(value, JsonType::Array) || value->array.size() != 4)
        return std::nullopt;
    core::Color result;
    unsigned char* components[] = {&result.red, &result.green, &result.blue, &result.alpha};
    for (std::size_t index{}; index < value->array.size(); ++index)
    {
        const JsonValue& component = value->array[index];
        if (component.type != JsonType::Number || component.number > 255)
            return std::nullopt;
        *components[index] = static_cast<unsigned char>(component.number);
    }
    return result;
}

std::optional<int> ReadSizeComponent(const JsonValue* value)
{
    if (!IsType(value, JsonType::Number) || value->number > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))
        return std::nullopt;
    return static_cast<int>(value->number);
}

std::optional<DuiRichTextRun> ReadRun(const JsonValue& value)
{
    const JsonValue* text = FindMember(value, "text");
    const JsonValue* color = FindMember(value, "color");
    const JsonValue* bold = FindMember(value, "bold");
    const JsonValue* italic = FindMember(value, "italic");
    const JsonValue* underline = FindMember(value, "underline");
    const JsonValue* link = FindMember(value, "link");
    if (!IsType(text, JsonType::String) || !IsType(bold, JsonType::Boolean) || !IsType(italic, JsonType::Boolean)
        || !IsType(underline, JsonType::Boolean) || !IsType(link, JsonType::String))
        return std::nullopt;
    const std::optional<std::string> decodedText = DecodeUtf8(text->string);
    const std::optional<std::string> decodedLink = DecodeUtf8(link->string);
    const std::optional<core::Color> decodedColor = ReadColor(color);
    if (!decodedText || !decodedLink || !decodedColor)
        return std::nullopt;
    return DuiRichTextRun{std::move(*decodedText), {*decodedColor, bold->boolean, italic->boolean, underline->boolean},
        std::move(*decodedLink)};
}

std::optional<DuiRichDocumentBlock> ReadBlock(const JsonValue& value)
{
    const JsonValue* kind = FindMember(value, "kind");
    if (!IsType(kind, JsonType::String))
        return std::nullopt;
    DuiRichDocumentBlock result;
    if (kind->string == "text")
    {
        const JsonValue* runs = FindMember(value, "runs");
        if (!IsType(runs, JsonType::Array))
            return std::nullopt;
        result.kind = DuiRichDocumentBlockKind::Text;
        for (const JsonValue& run : runs->array)
        {
            const std::optional<DuiRichTextRun> decodedRun = ReadRun(run);
            if (!decodedRun)
                return std::nullopt;
            result.runs.push_back(*decodedRun);
        }
        return result;
    }
    if (kind->string == "quote")
    {
        const JsonValue* sender = FindMember(value, "sender");
        const JsonValue* body = FindMember(value, "body");
        if (!IsType(sender, JsonType::String) || !IsType(body, JsonType::String))
            return std::nullopt;
        const std::optional<std::string> decodedSender = DecodeUtf8(sender->string);
        const std::optional<std::string> decodedBody = DecodeUtf8(body->string);
        if (!decodedSender || !decodedBody)
            return std::nullopt;
        result.kind = DuiRichDocumentBlockKind::Quote;
        result.sender = std::move(*decodedSender);
        result.body = std::move(*decodedBody);
        return result;
    }
    if (kind->string == "file")
    {
        const JsonValue* name = FindMember(value, "name");
        const JsonValue* size = FindMember(value, "size");
        if (!IsType(name, JsonType::String) || !IsType(size, JsonType::Number))
            return std::nullopt;
        const std::optional<std::string> decodedName = DecodeUtf8(name->string);
        if (!decodedName)
            return std::nullopt;
        result.kind = DuiRichDocumentBlockKind::FileCard;
        result.fileName = std::move(*decodedName);
        result.fileSize = size->number;
        return result;
    }
    if (kind->string == "image")
    {
        const JsonValue* resource = FindMember(value, "resource");
        const std::optional<int> width = ReadSizeComponent(FindMember(value, "width"));
        const std::optional<int> height = ReadSizeComponent(FindMember(value, "height"));
        if (!IsType(resource, JsonType::String) || resource->string.empty() || !width || !height)
            return std::nullopt;
        result.kind = DuiRichDocumentBlockKind::Image;
        result.imageResourceEntry = resource->string;
        result.imageSize = {*width, *height};
        return result;
    }
    return std::nullopt;
}

} // namespace

std::string DuiRichDocumentJson::Serialize(const DuiRichDocument& document)
{
    std::string result{"{\"version\":1,\"blocks\":["};
    bool firstBlock = true;
    for (const DuiRichDocumentBlock& block : document.Blocks())
    {
        if (!firstBlock)
            result.push_back(',');
        firstBlock = false;
        switch (block.kind)
        {
        case DuiRichDocumentBlockKind::Text:
            result += "{\"kind\":\"text\",\"runs\":[";
            for (std::size_t index{}; index < block.runs.size(); ++index)
            {
                if (index != 0)
                    result.push_back(',');
                const DuiRichTextRun& run = block.runs[index];
                result += "{\"text\":";
                AppendUtf16String(result, run.text);
                result += ",\"color\":";
                AppendColor(result, run.format.color);
                result += ",\"bold\":";
                result += run.format.bold ? "true" : "false";
                result += ",\"italic\":";
                result += run.format.italic ? "true" : "false";
                result += ",\"underline\":";
                result += run.format.underline ? "true" : "false";
                result += ",\"link\":";
                AppendUtf16String(result, run.link);
                result.push_back('}');
            }
            result += "]}";
            break;
        case DuiRichDocumentBlockKind::Quote:
            result += "{\"kind\":\"quote\",\"sender\":";
            AppendUtf16String(result, block.sender);
            result += ",\"body\":";
            AppendUtf16String(result, block.body);
            result.push_back('}');
            break;
        case DuiRichDocumentBlockKind::FileCard:
            result += "{\"kind\":\"file\",\"name\":";
            AppendUtf16String(result, block.fileName);
            result += ",\"size\":";
            result += std::to_string(block.fileSize);
            result.push_back('}');
            break;
        case DuiRichDocumentBlockKind::Image:
            result += "{\"kind\":\"image\",\"resource\":";
            AppendEscaped(result, block.imageResourceEntry);
            result += ",\"width\":";
            result += std::to_string(block.imageSize.width);
            result += ",\"height\":";
            result += std::to_string(block.imageSize.height);
            result.push_back('}');
            break;
        }
    }
    result += "]}";
    return result;
}

std::optional<DuiRichDocument> DuiRichDocumentJson::Deserialize(std::string_view json)
{
    const std::optional<JsonValue> root = JsonReader(json).Read();
    const JsonValue* version = root ? FindMember(*root, "version") : nullptr;
    const JsonValue* blocks = root ? FindMember(*root, "blocks") : nullptr;
    if (!version || !blocks || !IsType(version, JsonType::Number) || version->number != 1
        || !IsType(blocks, JsonType::Array))
        return std::nullopt;
    DuiRichDocument result;
    for (const JsonValue& value : blocks->array)
    {
        const std::optional<DuiRichDocumentBlock> block = ReadBlock(value);
        if (!block)
            return std::nullopt;
        switch (block->kind)
        {
        case DuiRichDocumentBlockKind::Text:
            result.AddTextBlock(block->runs);
            break;
        case DuiRichDocumentBlockKind::Quote:
            result.AddQuoteBlock(block->sender, block->body);
            break;
        case DuiRichDocumentBlockKind::FileCard:
            result.AddFileCard(block->fileName, block->fileSize);
            break;
        case DuiRichDocumentBlockKind::Image:
            result.AddImageBlock(block->imageResourceEntry, block->imageSize);
            break;
        }
    }
    return result;
}

} // namespace ysDui::controls::input
