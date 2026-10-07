#include "ysDui/platform/win32/DuiWin32FontRegistry.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <mutex>
#include <utility>

#include <dwrite.h>
#include <dwrite_3.h>
#include <windows.h>
#include <wrl/client.h>

#include "DuiWin32RegisteredFonts.hpp"
#include "DuiWin32Utf8.hpp"

namespace ysDui::platform::win32 {
namespace {

using Microsoft::WRL::ComPtr;

std::vector<std::wstring> MemoryFontFamilies(
    const std::shared_ptr<const std::vector<std::uint8_t>>& bytes)
{
    std::vector<std::wstring> result;
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteFactory3> factory3;
    ComPtr<IDWriteFactory5> factory5;
    ComPtr<IDWriteInMemoryFontFileLoader> loader;
    ComPtr<IDWriteFontFile> fontFile;
    ComPtr<IDWriteFontSetBuilder> baseBuilder;
    ComPtr<IDWriteFontSetBuilder1> builder;
    ComPtr<IDWriteFontSet> fontSet;
    ComPtr<IDWriteFontCollection1> collection;
    bool loaderRegistered{};

    if (bytes->size() <= (std::numeric_limits<UINT32>::max)()
        && SUCCEEDED(::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.GetAddressOf())))
        && SUCCEEDED(factory.As(&factory3))
        && SUCCEEDED(factory.As(&factory5))
        && SUCCEEDED(factory5->CreateInMemoryFontFileLoader(&loader))
        && SUCCEEDED(factory->RegisterFontFileLoader(loader.Get())))
    {
        loaderRegistered = true;
        if (SUCCEEDED(loader->CreateInMemoryFontFileReference(
                factory.Get(), bytes->data(), static_cast<UINT32>(bytes->size()),
                nullptr, &fontFile))
            && SUCCEEDED(factory3->CreateFontSetBuilder(&baseBuilder))
            && SUCCEEDED(baseBuilder.As(&builder))
            && SUCCEEDED(builder->AddFontFile(fontFile.Get()))
            && SUCCEEDED(builder->CreateFontSet(&fontSet))
            && SUCCEEDED(factory3->CreateFontCollectionFromFontSet(fontSet.Get(), &collection)))
        {
            for (UINT32 index = 0; index < collection->GetFontFamilyCount(); ++index)
            {
                ComPtr<IDWriteFontFamily> fontFamily;
                ComPtr<IDWriteLocalizedStrings> names;
                UINT32 length{};
                if (FAILED(collection->GetFontFamily(index, &fontFamily))
                    || FAILED(fontFamily->GetFamilyNames(&names))
                    || names->GetCount() == 0
                    || FAILED(names->GetStringLength(0, &length)))
                {
                    continue;
                }
                std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
                if (SUCCEEDED(names->GetString(0, name.data(), length + 1)))
                {
                    name.resize(length);
                    result.push_back(std::move(name));
                }
            }
        }
    }

    collection.Reset();
    fontSet.Reset();
    builder.Reset();
    baseBuilder.Reset();
    fontFile.Reset();
    if (loaderRegistered) (void)factory->UnregisterFontFileLoader(loader.Get());
    return result;
}

bool SameFamily(const std::wstring& registered, std::wstring_view requested)
{
    if (registered.size() != requested.size()) return false;
    return ::CompareStringOrdinal(registered.data(), static_cast<int>(registered.size()),
                                  requested.data(), static_cast<int>(requested.size()), TRUE)
        == CSTR_EQUAL;
}

} // namespace

namespace registered_fonts {
namespace {

struct FileEntry final
{
    Token token{};
    std::wstring path;
};

struct MemoryEntry final
{
    Token token{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

std::mutex FilesMutex;
std::vector<FileEntry> Files;
std::vector<MemoryEntry> MemoryFiles;
Token NextToken{1};
std::size_t Generation{};

} // namespace

Token AddFile(std::wstring path)
{
    std::lock_guard lock(FilesMutex);
    const Token token = NextToken++;
    Files.push_back({token, std::move(path)});
    ++Generation;
    return token;
}

Token AddMemory(std::shared_ptr<const std::vector<std::uint8_t>> bytes)
{
    std::lock_guard lock(FilesMutex);
    const Token token = NextToken++;
    MemoryFiles.push_back({token, std::move(bytes)});
    ++Generation;
    return token;
}

void Remove(Token token)
{
    std::lock_guard lock(FilesMutex);
    std::erase_if(Files, [token](const FileEntry& entry) { return entry.token == token; });
    std::erase_if(MemoryFiles, [token](const MemoryEntry& entry) { return entry.token == token; });
    ++Generation;
}

Snapshot FontSnapshot()
{
    std::lock_guard lock(FilesMutex);
    Snapshot snapshot;
    snapshot.generation = Generation;
    snapshot.files.reserve(Files.size());
    for (const auto& entry : Files) snapshot.files.push_back(entry.path);
    snapshot.memoryFiles.reserve(MemoryFiles.size());
    for (const auto& entry : MemoryFiles) snapshot.memoryFiles.push_back(entry.bytes);
    return snapshot;
}

} // namespace registered_fonts

class Win32FontRegistry::Impl {
public:
    struct FileFont { std::string path; registered_fonts::Token token{}; };
    struct MemoryFont {
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        std::vector<std::wstring> families;
        ::HANDLE handle{};
        registered_fonts::Token token{};
    };
    std::vector<FileFont> files;
    std::vector<MemoryFont> memoryFonts;
};
Win32FontRegistry::Win32FontRegistry() : impl_(std::make_unique<Impl>()) {}
Win32FontRegistry::~Win32FontRegistry() {
    for (const auto& font : impl_->files) {
        registered_fonts::Remove(font.token);
        ::RemoveFontResourceExW(detail::Utf8ToWide(font.path).c_str(), FR_PRIVATE, nullptr);
    }
    for (const auto& font : impl_->memoryFonts) {
        registered_fonts::Remove(font.token);
        if (font.handle) ::RemoveFontMemResourceEx(font.handle);
    }
}
bool Win32FontRegistry::RegisterFile(std::string path) {
    const std::wstring nativePath = detail::Utf8ToWide(path);
    if (nativePath.empty() || !::AddFontResourceExW(nativePath.c_str(), FR_PRIVATE, nullptr)) return false;
    const std::wstring absolutePath = std::filesystem::absolute(nativePath).wstring();
    impl_->files.push_back({std::move(path), registered_fonts::AddFile(absolutePath)});
    return true;
}
bool Win32FontRegistry::RegisterMemory(std::vector<std::uint8_t> bytes) {
    if (bytes.empty()) return false;
    auto storage = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
    ::DWORD count{};
    ::HANDLE handle = ::AddFontMemResourceEx(const_cast<std::uint8_t*>(storage->data()),
                                             static_cast<::DWORD>(storage->size()), nullptr, &count);
    if (!handle || count == 0) return false;
    impl_->memoryFonts.push_back(
        {storage, MemoryFontFamilies(storage), handle, registered_fonts::AddMemory(storage)});
    return true;
}
bool Win32FontRegistry::HasFamily(std::string_view family) const {
    if (family.empty()) return false;
    const std::wstring nativeFamily = detail::Utf8ToWide(family);
    for (const auto& font : impl_->memoryFonts) {
        if (std::any_of(font.families.begin(), font.families.end(),
                        [&nativeFamily](const std::wstring& registered) {
                            return SameFamily(registered, nativeFamily);
                        }))
            return true;
    }
    ::HDC context = ::GetDC(nullptr);
    if (!context) return false;
    bool found{};
    ::EnumFontFamiliesW(context, nativeFamily.c_str(),
                        [] (const ::LOGFONTW*, const ::TEXTMETRICW*, ::DWORD, ::LPARAM data) {
                            *reinterpret_cast<bool*>(data) = true;
                            return 0;
                        }, reinterpret_cast<::LPARAM>(&found));
    ::ReleaseDC(nullptr, context);
    return found;
}
} // namespace ysDui::platform::win32
