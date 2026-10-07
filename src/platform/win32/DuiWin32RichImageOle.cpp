#include "DuiWin32RichImageOle.hpp"

#include <ole2.h>
#include <richedit.h>
#include <richole.h>

namespace ysDui::platform::win32 {
namespace {
class RichImageOle final : public IOleObject, public IViewObject2, public IDataObject, public IPersistStorage {
public:
    explicit RichImageOle(HBITMAP bitmap) : bitmap_(bitmap) { BITMAP info{}; if (::GetObject(bitmap_, sizeof(info), &info) == sizeof(info)) { pixels_ = {info.bmWidth, info.bmHeight}; extent_ = {::MulDiv(info.bmWidth, 2540, 96), ::MulDiv(info.bmHeight, 2540, 96)}; } }
    ~RichImageOle() { if (site_) site_->Release(); if (bitmap_) ::DeleteObject(bitmap_); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override { if (!result) return E_POINTER; *result = nullptr; if (id == IID_IUnknown || id == IID_IOleObject) *result = static_cast<IOleObject*>(this); else if (id == IID_IViewObject || id == IID_IViewObject2) *result = static_cast<IViewObject2*>(this); else if (id == IID_IDataObject) *result = static_cast<IDataObject*>(this); else if (id == IID_IPersist || id == IID_IPersistStorage) *result = static_cast<IPersistStorage*>(this); if (!*result) return E_NOINTERFACE; AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(::InterlockedIncrement(&references_)); }
    ULONG STDMETHODCALLTYPE Release() override { const LONG count = ::InterlockedDecrement(&references_); if (!count) delete this; return static_cast<ULONG>(count); }
    HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite* site) override { if (site_) site_->Release(); site_ = site; if (site_) site_->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite** result) override { if (!result) return E_POINTER; *result = site_; if (site_) site_->AddRef(); return S_OK; }
    HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR, LPCOLESTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Close(DWORD) override { if (site_) { site_->Release(); site_ = nullptr; } return S_OK; }
    HRESULT STDMETHODCALLTYPE SetMoniker(DWORD, IMoniker*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE InitFromData(IDataObject*, BOOL, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD, IDataObject**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DoVerb(LONG, LPMSG, IOleClientSite*, LONG, HWND, LPCRECT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Update() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE IsUpToDate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID* result) override { if (result) *result = CLSID_NULL; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetUserType(DWORD, LPOLESTR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetExtent(DWORD, SIZEL* value) override { if (value) extent_ = *value; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, SIZEL* result) override { if (!result) return E_POINTER; *result = extent_; return S_OK; }
    HRESULT STDMETHODCALLTYPE Advise(IAdviseSink*, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD, DWORD* result) override { if (result) *result = OLEMISC_RECOMPOSEONRESIZE | OLEMISC_INSIDEOUT; return S_OK; }
    HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Draw(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, HDC target, LPCRECTL bounds, LPCRECTL, BOOL (STDMETHODCALLTYPE*)(ULONG_PTR), ULONG_PTR) override { if (!target || !bounds || !bitmap_) return E_INVALIDARG; HDC memory = ::CreateCompatibleDC(target); if (!memory) return E_FAIL; HGDIOBJ old = ::SelectObject(memory, bitmap_); const int width = bounds->right - bounds->left; const int height = bounds->bottom - bounds->top; if (pixels_.cx && pixels_.cy && width > 0 && height > 0) ::StretchBlt(target, bounds->left, bounds->top, width, height, memory, 0, 0, pixels_.cx, pixels_.cy, SRCCOPY); ::SelectObject(memory, old); ::DeleteDC(memory); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetColorSet(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LOGPALETTE**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Freeze(DWORD, LONG, void*, DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Unfreeze(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetAdvise(DWORD, DWORD, IAdviseSink*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetAdvise(DWORD* aspect, DWORD* flags, IAdviseSink** sink) override { if (aspect) *aspect = 0; if (flags) *flags = 0; if (sink) *sink = nullptr; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, LONG, DVTARGETDEVICE*, LPSIZEL result) override { if (!result) return E_POINTER; *result = extent_; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override { if (!format || !medium) return E_POINTER; if (format->cfFormat != CF_BITMAP) return DV_E_FORMATETC; if (!(format->tymed & TYMED_GDI)) return DV_E_TYMED; medium->tymed = TYMED_GDI; medium->hBitmap = static_cast<HBITMAP>(::CopyImage(bitmap_, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG)); medium->pUnkForRelease = nullptr; return medium->hBitmap ? S_OK : E_FAIL; }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override { return format && format->cfFormat == CF_BITMAP && (format->tymed & TYMED_GDI) ? S_OK : S_FALSE; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* result) override { if (result) { ::ZeroMemory(result, sizeof(*result)); result->cfFormat = CF_BITMAP; result->dwAspect = DVASPECT_CONTENT; result->lindex = -1; result->tymed = TYMED_GDI; } return DATA_S_SAMEFORMATETC; }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* result) override { if (result) *result = CLSID_NULL; return S_OK; }
    HRESULT STDMETHODCALLTYPE IsDirty() override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE InitNew(IStorage*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Load(IStorage*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Save(IStorage*, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SaveCompleted(IStorage*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE HandsOffStorage() override { return S_OK; }
    [[nodiscard]] SIZEL Extent() const { return extent_; }
private:
    LONG references_{1}; HBITMAP bitmap_{}; SIZE pixels_{}; SIZEL extent_{}; IOleClientSite* site_{};
};
}

bool InsertRichImageOle(HWND richEdit, HBITMAP bitmap)
{
    if (!richEdit || !bitmap) return false;
    IRichEditOle* richOle{};
    ::SendMessageW(richEdit, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&richOle));
    if (!richOle) { ::DeleteObject(bitmap); return false; }
    IOleClientSite* site{};
    richOle->GetClientSite(&site);
    ILockBytes* lockBytes{};
    IStorage* storage{};
    if (FAILED(::CreateILockBytesOnHGlobal(nullptr, TRUE, &lockBytes)) ||
        FAILED(::StgCreateDocfileOnILockBytes(lockBytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &storage))) {
        if (storage) storage->Release(); if (lockBytes) lockBytes->Release(); if (site) site->Release(); richOle->Release(); ::DeleteObject(bitmap); return false;
    }
    auto* object = new RichImageOle(bitmap);
    object->SetClientSite(site);
    ::OleSetContainedObject(static_cast<IOleObject*>(object), TRUE);
    REOBJECT value{};
    value.cbStruct = sizeof(value); value.clsid = CLSID_NULL; value.cp = REO_CP_SELECTION; value.dvaspect = DVASPECT_CONTENT; value.dwFlags = REO_BELOWBASELINE; value.poleobj = static_cast<IOleObject*>(object); value.pstg = storage; value.polesite = site; value.sizel = object->Extent();
    const HRESULT result = richOle->InsertObject(&value);
    object->Release(); storage->Release(); lockBytes->Release(); if (site) site->Release(); richOle->Release();
    return SUCCEEDED(result);
}

} // namespace ysDui::platform::win32
