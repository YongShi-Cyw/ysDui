/**
 * 文件名：DuiResourcePack.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：提供 CMake 构建的 ZIP 资源包命令行打包工具。
 */
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "DuiUtf8Path.hpp"

#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
#include <sodium.h>
#endif

namespace {

bool ReadFile(const std::filesystem::path& path, std::vector<char>& bytes)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return false;
    const std::streamsize length = input.tellg();
    if (length < 0)
        return false;
    bytes.resize(static_cast<std::size_t>(length));
    input.seekg(0);
    return length == 0 || static_cast<bool>(input.read(bytes.data(), length));
}

bool ReadKeyFile(const std::filesystem::path& path, std::array<std::uint8_t, 32>& key)
{
    std::ifstream input(path, std::ios::binary);
    return input && static_cast<bool>(input.read(reinterpret_cast<char*>(key.data()), key.size()))
        && input.peek() == std::char_traits<char>::eof();
}

bool EncryptEntry(std::vector<char>& bytes, std::string_view name, const std::optional<std::array<std::uint8_t, 32>>& key)
{
    if (!key)
        return true;
#if defined(YSDUI_RESOURCE_WITH_SODIUM)
    constexpr std::array<char, 8> MAGIC{'Y', 'S', 'D', 'U', 'I', 'E', 'N', 'C'};
    if (sodium_init() < 0)
        return false;
    std::vector<char> encrypted(MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + bytes.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
    std::copy(MAGIC.begin(), MAGIC.end(), encrypted.begin());
    unsigned char* nonce = reinterpret_cast<unsigned char*>(encrypted.data() + MAGIC.size());
    randombytes_buf(nonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    unsigned long long encryptedSize{};
    const int result = crypto_aead_xchacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char*>(encrypted.data() + MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES),
        &encryptedSize, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(),
        reinterpret_cast<const unsigned char*>(name.data()), name.size(), nullptr, nonce, key->data());
    if (result != 0)
        return false;
    encrypted.resize(MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + static_cast<std::size_t>(encryptedSize));
    bytes = std::move(encrypted);
    return true;
#else
    (void)bytes;
    (void)name;
    return false;
#endif
}

bool AddEntry(void* writer, const std::string& name, const std::filesystem::path& path,
    const std::optional<std::array<std::uint8_t, 32>>& key)
{
    if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos)
        return false;
    std::vector<char> bytes;
    if (!ReadFile(path, bytes) || !EncryptEntry(bytes, name, key))
        return false;

    mz_zip_writer_set_compress_method(writer, key ? MZ_COMPRESS_METHOD_STORE : MZ_COMPRESS_METHOD_DEFLATE);
    mz_zip_file file{};
    file.filename = name.c_str();
    return mz_zip_writer_add_buffer(writer, bytes.data(), static_cast<int32_t>(bytes.size()), &file) == MZ_OK;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: ysdui_resource_pack <output.zip> [--key-file=key.bin] <entry=source>...\n";
        return 1;
    }

    std::optional<std::array<std::uint8_t, 32>> key;
    int index = 2;
    if (std::string_view(argv[index]).starts_with("--key-file="))
    {
        key.emplace();
        const auto keyPath = ysDui::core::detail::PathFromUtf8(argv[index] + 11);
        if (!keyPath || !ReadKeyFile(*keyPath, *key))
            return 1;
        ++index;
    }
    if (index == argc)
        return 1;

    void* writer = mz_zip_writer_create();
    const auto output = ysDui::core::detail::PathFromUtf8(argv[1]);
    const std::string outputPath = output ? ysDui::core::detail::PathToUtf8(*output) : std::string{};
    if (!writer || !output || mz_zip_writer_open_file(writer, outputPath.c_str(), 0, 0) != MZ_OK)
    {
        if (writer)
            mz_zip_writer_delete(&writer);
        return 1;
    }

    mz_zip_writer_set_compress_level(writer, MZ_COMPRESS_LEVEL_DEFAULT);
    bool succeeded = true;
    for (; index < argc && succeeded; ++index)
    {
        const std::string argument(argv[index]);
        const std::size_t separator = argument.find('=');
        const auto source = separator == std::string::npos
            ? std::nullopt : ysDui::core::detail::PathFromUtf8(argument.substr(separator + 1));
        succeeded = separator != std::string::npos && separator > 0
            && source && AddEntry(writer, argument.substr(0, separator), *source, key);
    }

    succeeded = mz_zip_writer_close(writer) == MZ_OK && succeeded;
    mz_zip_writer_delete(&writer);
    return succeeded ? 0 : 1;
}
