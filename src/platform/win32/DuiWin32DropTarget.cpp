/**
 * 文件名：DuiWin32DropTarget.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：实现 Win32 OLE 拖放到平台无关拖放载荷的转换。
 */
#include "ysDui/platform/win32/DuiWin32DropTarget.hpp"
#include "DuiWin32Utf8.hpp"
#include "DuiImageAccess.hpp"

#include <windows.h>
#include <ole2.h>
#include <shellapi.h>

#include <atomic>
#include <cstring>
#include <utility>
#include <vector>

namespace ysDui::platform::win32 {
class Win32DropTargetImpl final : public ::IDropTarget
{
public:
    explicit Win32DropTargetImpl(::HWND host) : host_(host) {}

    ::HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** result) override
    {
        if (!result)
            return E_POINTER;
        if (identifier == IID_IUnknown || identifier == IID_IDropTarget)
        {
            *result = static_cast<::IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *result = nullptr;
        return E_NOINTERFACE;
    }

    ::ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }

    ::ULONG STDMETHODCALLTYPE Release() override { return --references_; }

    ::HRESULT STDMETHODCALLTYPE DragEnter(::IDataObject* data, ::DWORD, ::POINTL, ::DWORD* effect) override
    {
        canDrop_ = enabled_ && Supports(data);
        SetEffect(effect);
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE DragOver(::DWORD, ::POINTL, ::DWORD* effect) override
    {
        SetEffect(effect);
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        canDrop_ = false;
        return S_OK;
    }

    ::HRESULT STDMETHODCALLTYPE Drop(::IDataObject* data, ::DWORD, ::POINTL, ::DWORD* effect) override
    {
        if (effect)
            *effect = DROPEFFECT_NONE;
        if (!enabled_ || !data)
            return S_OK;
        if (DropFiles(data) || DropBitmap(data))
        {
            if (effect)
                *effect = DROPEFFECT_COPY;
        }
        canDrop_ = false;
        return S_OK;
    }

    void SetHandler(ui::DuiDropTarget::Handler handler) { handler_ = std::move(handler); }
    void SetEnabled(bool enabled)
    {
        enabled_ = enabled;
        if (!enabled_)
            canDrop_ = false;
    }

private:
    static ::FORMATETC Format(::CLIPFORMAT format, ::DWORD storage)
    {
        return {format, nullptr, DVASPECT_CONTENT, -1, storage};
    }

    static bool Supports(::IDataObject* data)
    {
        if (!data)
            return false;
        auto files = Format(CF_HDROP, TYMED_HGLOBAL);
        if (data->QueryGetData(&files) == S_OK)
            return true;
        auto bitmap = Format(CF_BITMAP, TYMED_GDI);
        return data->QueryGetData(&bitmap) == S_OK;
    }

    void SetEffect(::DWORD* effect) const
    {
        if (effect)
            *effect = canDrop_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    }

    bool DropFiles(::IDataObject* data)
    {
        auto format = Format(CF_HDROP, TYMED_HGLOBAL);
        ::STGMEDIUM medium{};
        if (data->GetData(&format, &medium) != S_OK)
            return false;
        auto drop = static_cast<::HDROP>(::GlobalLock(medium.hGlobal));
        ui::DuiDropFiles files;
        if (drop)
        {
            const ::UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            files.paths.reserve(count);
            for (::UINT index = 0; index < count; ++index)
            {
                const ::UINT length = ::DragQueryFileW(drop, index, nullptr, 0);
                if (length == 0)
                    continue;
                std::wstring nativePath(static_cast<std::size_t>(length) + 1, L'\0');
                if (::DragQueryFileW(drop, index, nativePath.data(), length + 1) != 0)
                {
                    nativePath.resize(length);
                    files.paths.push_back(detail::WideToUtf8(nativePath));
                }
            }
            ::GlobalUnlock(medium.hGlobal);
        }
        ::ReleaseStgMedium(&medium);
        if (files.paths.empty())
            return false;
        Notify(std::move(files));
        return true;
    }

    bool DropBitmap(::IDataObject* data)
    {
        auto format = Format(CF_BITMAP, TYMED_GDI);
        ::STGMEDIUM medium{};
        if (data->GetData(&format, &medium) != S_OK)
            return false;
        auto image = DecodeBitmap(medium.hBitmap);
        ::ReleaseStgMedium(&medium);
        if (!image)
            return false;
        Notify(ui::DuiDropImage{std::move(image)});
        return true;
    }

    std::shared_ptr<const render::DuiImage> DecodeBitmap(::HBITMAP bitmap) const
    {
        if (!bitmap)
            return {};
        ::BITMAP native{};
        if (::GetObjectW(bitmap, sizeof(native), &native) != sizeof(native) || native.bmWidth <= 0 || native.bmHeight <= 0)
            return {};
        const int width = native.bmWidth;
        const int height = native.bmHeight < 0 ? -native.bmHeight : native.bmHeight;
        ::BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
        const auto screen = ::GetDC(nullptr);
        const int lines = ::GetDIBits(screen, bitmap, 0, static_cast<::UINT>(height), pixels.data(), &info, DIB_RGB_COLORS);
        ::ReleaseDC(nullptr, screen);
        if (lines != height)
            return {};
        return std::make_shared<const render::DuiImage>(render::DuiImageAccess::Create(
            {width, height}, render::DuiImageFormat::Bgra8Premultiplied, std::move(pixels)));
    }

    void Notify(ui::DuiDropPayload payload) const
    {
        if (handler_)
            handler_(payload);
    }

    std::atomic<::ULONG> references_{1};
    ::HWND host_{};
    ui::DuiDropTarget::Handler handler_;
    bool enabled_{true};
    bool canDrop_{};
};

class Win32DropTarget::Impl final
{
public:
    explicit Impl(::HWND host) : host_(host), target_(std::make_unique<Win32DropTargetImpl>(host)) {}

    ::HWND host_{};
    std::unique_ptr<Win32DropTargetImpl> target_;
};

Win32DropTarget::Win32DropTarget(NativeWindowHandle host)
{
    const auto handle = reinterpret_cast<::HWND>(host.value);
    if (!handle || !::IsWindow(handle))
        return;
    auto implementation = std::make_unique<Impl>(handle);
    if (::RegisterDragDrop(handle, implementation->target_.get()) == S_OK)
        impl_ = std::move(implementation);
}

Win32DropTarget::~Win32DropTarget()
{
    if (impl_)
        ::RevokeDragDrop(impl_->host_);
}

Win32DropTarget::Win32DropTarget(Win32DropTarget&&) noexcept = default;
Win32DropTarget& Win32DropTarget::operator=(Win32DropTarget&&) noexcept = default;

void Win32DropTarget::SetHandler(Handler handler)
{
    if (impl_)
        impl_->target_->SetHandler(std::move(handler));
}

void Win32DropTarget::SetEnabled(bool enabled)
{
    if (impl_)
        impl_->target_->SetEnabled(enabled);
}

} // namespace ysDui::platform::win32
