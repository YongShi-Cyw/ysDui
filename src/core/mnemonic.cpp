#include "ysDui/core/DuiMnemonic.hpp"

#include <cwctype>

namespace ysDui::core::mnemonic {

char FindCharacter(std::string_view text)
{
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '&') continue;
        if (index + 1 >= text.size()) return {};
        const char next = text[index + 1];
        if (next == '&') {
            ++index;
            continue;
        }
        return static_cast<char>(std::towlower(static_cast<wint_t>(next)));
    }
    return {};
}

std::string StripPrefix(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '&') {
            result.push_back(text[index]);
            continue;
        }
        if (index + 1 < text.size() && text[index + 1] == '&') {
            result.push_back('&');
            ++index;
        }
    }
    return result;
}

} // namespace ysDui::core::mnemonic
