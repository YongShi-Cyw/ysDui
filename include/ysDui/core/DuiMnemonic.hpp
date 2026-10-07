#pragma once

#include <string>
#include <string_view>

namespace ysDui::core::mnemonic {

[[nodiscard]] char FindCharacter(std::string_view text);
[[nodiscard]] std::string StripPrefix(std::string_view text);

} // namespace ysDui::core::mnemonic
