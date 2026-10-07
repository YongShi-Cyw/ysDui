/**
 * 文件名：DuiResourcePackage.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：使用 minizip-ng 实现 ZIP 资源包读取。
 */
#include "ysDui/resource/DuiResourcePackage.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include "DuiUtf8Path.hpp"

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
#include <sodium.h>
#endif

namespace ysDui::resource {

class DuiResourcePackage::Impl final
{
public:
    Impl(std::string path, std::optional<DuiResourceKey> key)
        : path_(std::move(path)), key_(std::move(key)) {}

    std::string path_;
    std::optional<DuiResourceKey> key_;
};

namespace {

void* OpenReader(const std::string& path)
{
    void* reader = mz_zip_reader_create();
    if (!reader || mz_zip_reader_open_file(reader, path.c_str()) != MZ_OK)
    {
        if (reader)
            mz_zip_reader_delete(&reader);
        return nullptr;
    }
    return reader;
}

void CloseReader(void*& reader)
{
    if (!reader)
        return;
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
}

constexpr std::array<std::uint8_t, 8> ENCRYPTED_ENTRY_MAGIC{
    'Y', 'S', 'D', 'u', 'I', 'E', 'N', 'C'};

std::optional<std::vector<std::uint8_t>> DecryptEntry(
    std::vector<std::uint8_t> bytes, std::string_view name, const std::optional<DuiResourceKey>& key)
{
    if (bytes.size() < ENCRYPTED_ENTRY_MAGIC.size()
        || !std::equal(ENCRYPTED_ENTRY_MAGIC.begin(), ENCRYPTED_ENTRY_MAGIC.end(), bytes.begin()))
        return bytes;

#if defined(YSDUI_RESOURCE_WITH_SODIUM)
    constexpr std::size_t NONCE_SIZE = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    constexpr std::size_t TAG_SIZE = crypto_aead_xchacha20poly1305_ietf_ABYTES;
    const std::size_t payloadOffset = ENCRYPTED_ENTRY_MAGIC.size() + NONCE_SIZE;
    if (!key || bytes.size() < payloadOffset + TAG_SIZE || sodium_init() < 0)
        return std::nullopt;

    std::vector<std::uint8_t> plaintext(bytes.size() - payloadOffset - TAG_SIZE);
    unsigned long long plaintextSize{};
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(plaintext.data(), &plaintextSize, nullptr,
            bytes.data() + payloadOffset, bytes.size() - payloadOffset,
            reinterpret_cast<const unsigned char*>(name.data()), name.size(),
            bytes.data() + ENCRYPTED_ENTRY_MAGIC.size(), key->data()) != 0)
        return std::nullopt;
    plaintext.resize(static_cast<std::size_t>(plaintextSize));
    return plaintext;
#else
    (void)name;
    (void)key;
    return std::nullopt;
#endif
}

} // namespace

DuiResourcePackage::DuiResourcePackage(std::unique_ptr<Impl> implementation)
    : implementation_(std::move(implementation)) {}
DuiResourcePackage::~DuiResourcePackage() = default;
DuiResourcePackage::DuiResourcePackage(DuiResourcePackage&&) noexcept = default;
DuiResourcePackage& DuiResourcePackage::operator=(DuiResourcePackage&&) noexcept = default;

std::optional<DuiResourcePackage> DuiResourcePackage::Open(
    std::string_view path, std::optional<DuiResourceKey> key)
{
    if (path.empty())
        return std::nullopt;

    const auto nativePath = core::detail::PathFromUtf8(path);
    if (!nativePath)
        return std::nullopt;

    const std::string utf8Path = core::detail::PathToUtf8(*nativePath);
    void* reader = OpenReader(utf8Path);
    if (!reader)
        return std::nullopt;
    CloseReader(reader);
    return DuiResourcePackage(std::make_unique<Impl>(utf8Path, std::move(key)));
}

bool DuiResourcePackage::Contains(std::string_view name) const
{
    const std::vector<std::string> entries = Entries();
    return std::find(entries.begin(), entries.end(), name) != entries.end();
}

std::optional<std::vector<std::uint8_t>> DuiResourcePackage::Read(std::string_view name) const
{
    if (!implementation_ || name.empty())
        return std::nullopt;
    void* reader = OpenReader(implementation_->path_);
    if (!reader || mz_zip_reader_goto_first_entry(reader) != MZ_OK)
    {
        CloseReader(reader);
        return std::nullopt;
    }

    do
    {
        mz_zip_file* file{};
        if (mz_zip_reader_entry_get_info(reader, &file) != MZ_OK || !file || !file->filename
            || std::string(file->filename) != name)
            continue;
        if (mz_zip_reader_entry_open(reader) != MZ_OK)
        {
            CloseReader(reader);
            return std::nullopt;
        }

        const int32_t length = mz_zip_reader_entry_save_buffer_length(reader);
        if (length < 0)
        {
            mz_zip_reader_entry_close(reader);
            CloseReader(reader);
            return std::nullopt;
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        const int32_t result = length == 0 ? MZ_OK
            : mz_zip_reader_entry_save_buffer(reader, bytes.data(), length);
        mz_zip_reader_entry_close(reader);
        CloseReader(reader);
        if (result != MZ_OK)
            return std::nullopt;
        return DecryptEntry(std::move(bytes), name, implementation_->key_);
    } while (mz_zip_reader_goto_next_entry(reader) == MZ_OK);
    CloseReader(reader);
    return std::nullopt;
}

std::vector<std::string> DuiResourcePackage::Entries() const
{
    std::vector<std::string> entries;
    if (!implementation_)
        return entries;
    void* reader = OpenReader(implementation_->path_);
    if (!reader || mz_zip_reader_goto_first_entry(reader) != MZ_OK)
    {
        CloseReader(reader);
        return entries;
    }

    do
    {
        mz_zip_file* file{};
        if (mz_zip_reader_entry_get_info(reader, &file) == MZ_OK
            && file && file->filename && mz_zip_reader_entry_is_dir(reader) != MZ_OK)
            entries.emplace_back(file->filename);
    } while (mz_zip_reader_goto_next_entry(reader) == MZ_OK);
    CloseReader(reader);
    return entries;
}

} // namespace ysDui::resource
