#pragma once

#include <memory>

#include "ysDui/render/DuiFontRegistry.hpp"

namespace ysDui::platform::win32 {

class Win32FontRegistry final : public render::DuiFontRegistry {
public:
    Win32FontRegistry();
    ~Win32FontRegistry() override;
    Win32FontRegistry(const Win32FontRegistry&) = delete;
    Win32FontRegistry& operator=(const Win32FontRegistry&) = delete;
    bool RegisterFile(std::string path) override;
    bool RegisterMemory(std::vector<std::uint8_t> bytes) override;
    [[nodiscard]] bool HasFamily(std::string_view family) const override;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::platform::win32
