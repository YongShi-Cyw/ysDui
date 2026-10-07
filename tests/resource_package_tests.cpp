/**
 * 文件名：resource_package_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：验证 ZIP Deflate 资源包的读取协议。
 */
#include <cassert>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include "ysDui/resource/DuiResourcePackage.hpp"

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
#include <sodium.h>
#endif

namespace {

std::string ToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

bool CreatePackage(const std::filesystem::path& path)
{
    void* writer = mz_zip_writer_create();
    const std::string nativePath = ToUtf8(path);
    if (!writer || mz_zip_writer_open_file(writer, nativePath.c_str(), 0, 0) != MZ_OK)
    {
        if (writer)
            mz_zip_writer_delete(&writer);
        return false;
    }

    const std::string content = "resource-data";
    mz_zip_file entry{};
    entry.filename = "ui/logo.txt";
    mz_zip_writer_set_compress_method(writer, MZ_COMPRESS_METHOD_DEFLATE);
    mz_zip_writer_set_compress_level(writer, MZ_COMPRESS_LEVEL_DEFAULT);
    const bool written = mz_zip_writer_add_buffer(writer, content.data(),
        static_cast<int32_t>(content.size()), &entry) == MZ_OK;
    const bool closed = mz_zip_writer_close(writer) == MZ_OK;
    mz_zip_writer_delete(&writer);
    return written && closed;
}

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
bool CreateEncryptedPackage(const std::filesystem::path& path, const ysDui::resource::DuiResourceKey& key)
{
    constexpr std::array<std::uint8_t, 8> MAGIC{'Y', 'S', 'D', 'u', 'I', 'E', 'N', 'C'};
    const std::string name = "ui/secret.txt";
    const std::string content = "encrypted-resource";
    if (sodium_init() < 0)
        return false;
    std::vector<std::uint8_t> encrypted(MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + content.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
    std::copy(MAGIC.begin(), MAGIC.end(), encrypted.begin());
    unsigned char* nonce = encrypted.data() + MAGIC.size();
    randombytes_buf(nonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    unsigned long long encryptedSize{};
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            encrypted.data() + MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, &encryptedSize,
            reinterpret_cast<const unsigned char*>(content.data()), content.size(),
            reinterpret_cast<const unsigned char*>(name.data()), name.size(), nullptr, nonce, key.data()) != 0)
        return false;
    encrypted.resize(MAGIC.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + static_cast<std::size_t>(encryptedSize));

    void* writer = mz_zip_writer_create();
    const std::string nativePath = ToUtf8(path);
    if (!writer || mz_zip_writer_open_file(writer, nativePath.c_str(), 0, 0) != MZ_OK)
    {
        if (writer)
            mz_zip_writer_delete(&writer);
        return false;
    }
    mz_zip_file entry{};
    entry.filename = name.c_str();
    mz_zip_writer_set_compress_method(writer, MZ_COMPRESS_METHOD_STORE);
    const bool written = mz_zip_writer_add_buffer(writer, encrypted.data(),
        static_cast<int32_t>(encrypted.size()), &entry) == MZ_OK;
    const bool closed = mz_zip_writer_close(writer) == MZ_OK;
    mz_zip_writer_delete(&writer);
    return written && closed;
}
#endif

} // namespace

int main()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / std::filesystem::path(u8"ysdui-资源包测试.zip");
    std::filesystem::remove(path);
    assert(CreatePackage(path));

    const auto package = ysDui::resource::DuiResourcePackage::Open(ToUtf8(path));
    assert(package && package->Contains("ui/logo.txt"));
    assert(package->Entries() == std::vector<std::string>{"ui/logo.txt"});
    const auto bytes = package->Read("ui/logo.txt");
    assert(bytes && std::string(bytes->begin(), bytes->end()) == "resource-data");
    assert(!package->Read("ui/missing.txt"));
    std::filesystem::remove(path);

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
    const ysDui::resource::DuiResourceKey key{
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    assert(CreateEncryptedPackage(path, key));
    const auto encryptedPackage = ysDui::resource::DuiResourcePackage::Open(ToUtf8(path), key);
    const auto encryptedBytes = encryptedPackage ? encryptedPackage->Read("ui/secret.txt") : std::nullopt;
    assert(encryptedBytes && std::string(encryptedBytes->begin(), encryptedBytes->end()) == "encrypted-resource");
    const ysDui::resource::DuiResourceKey wrongKey{};
    const auto wrongKeyPackage = ysDui::resource::DuiResourcePackage::Open(ToUtf8(path), wrongKey);
    assert(wrongKeyPackage && !wrongKeyPackage->Read("ui/secret.txt"));
    std::filesystem::remove(path);
#endif
}
