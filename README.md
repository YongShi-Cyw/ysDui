# ysDui v2

ysDui v2 is a C++20 UI library being migrated from the `d1ab3f3` snapshot of BalloonUI. Its public headers are platform-neutral and contain no Windows SDK, COM, OLE, ATL, or WTL types.

## Modules

- `ysDui::core`: control tree, geometry, events, visual state, DPI values, and themes. It does not depend on rendering or a platform backend.
- `ysDui::render`: Canvas commands, text metrics, paths, images, and display lists. It depends only on core.
- `ysDui::ui`: platform-neutral host references and native capability contracts, including text input, clipboard, selectors, and native-view hosting.
- `ysDui::controls`: platform-neutral controls and layout containers. Controls use `core::Control` plus `render::DuiRenderable` when they paint.
- `ysDui::platform_win32`: optional Windows window host, event translation, DirectWrite/GDI/GDI+ Canvas implementation, DPI support, file dialogs, and image decoding.
- `ysDui::resource`: optional source-build ZIP resource package and packer.

Dependencies are `controls -> ui / render / core`, `ui -> render / core`, `render -> core`, `resource -> core`, and `platform_win32 -> controls / ui / render / core`. Only a platform backend may depend on all upper layers; the other modules never depend on a backend.

The source-snapshot mapping and unresolved semantic decisions are recorded in [docs/MIGRATION_COVERAGE.md](docs/MIGRATION_COVERAGE.md). The old Gallery-to-v2 page matrix, page-level status, and known interaction gaps are recorded in [docs/GALLERY_STYLE_MIGRATION.md](docs/GALLERY_STYLE_MIGRATION.md).
The module boundary, DPI, theme, host-reference, and backend-extension contracts are recorded in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
The UI-thread ownership, callback, background-work, and non-owning capability rules are recorded in [docs/THREADING.md](docs/THREADING.md).
Release history is recorded in [CHANGELOG.md](CHANGELOG.md), and dependency provenance and update rules are recorded in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).
The tag release prerequisites and artifact checks are recorded in [docs/RELEASE.md](docs/RELEASE.md).

## Public API Rules

Public stateful classes use PImpl. Public geometry and rendering values use explicit platform-neutral types such as `Point`, `Size`, `Rect`, `Color`, `Event`, and `DuiImage`. Native window handles, device contexts, GDI+ objects, and COM objects remain private to the Windows backend.

`DuiTextStyle::family` accepts a comma-separated UTF-8 font fallback list. The Win32 backend uses one DirectWrite text-layout path for UTF-8 shaping, bidirectional text, wrapping, font fallback, measurement, and drawing. The public Canvas protocol remains platform-neutral and does not expose DirectWrite types.

`DuiImageDecoder` is a Win32 capability that decodes PNG, JPEG, GIF, and BMP synchronously through GDI+. `DuiImage` exposes only size, format, and rendering behavior; decoded pixel storage is backend-private. `DuiAsyncImageLoader` performs control-layer asynchronous loading, coalescing, cancellation, and LRU caching while accepting a platform-provided synchronous decoder function; callers consume results through `Poll()` on their own UI thread. `DuiPixelBuffer` and `DuiGolden` provide backend-independent BGRA comparison and diff generation for rendering regression tests. `DuiWin32GoldenCodec` is the optional Win32 PNG file codec for these buffers; capture remains a backend responsibility.

`RasterizeSvg` uses the bundled NanoSVG implementation privately to turn UTF-8 SVG into a platform-neutral, premultiplied BGRA `DuiPixelBuffer`. The public API exposes neither NanoSVG nor a graphics-backend type. SVG parsing is intentionally a flat resource-to-pixels operation; it does not expose an editable SVG document model.

`DuiXmlDocument` privately embeds pugixml for small UTF-8 resource manifests and configuration documents. Its PImpl API exposes only root, direct-child, text, and attribute queries; it does not leak XML parser nodes or permit a platform dependency into public headers.

`core::DuiSkinCatalog` loads the legacy SkinList XML schema into `{id, name, resourcePrefix}` values. `core::DuiSkinSession` is an explicit per-application selection state that notifies on changes and resolves logical resource paths under the selected prefix. Image caches and decoded image ownership remain backend-private.

Theme, skin, and menu event subscriptions return the move-only `core::DuiSubscription` from their `*Scoped` entry points. The handle automatically unsubscribes on reset or destruction and remains safe when the publisher is destroyed first. The legacy `size_t` token and manual `Unsubscribe` APIs remain deprecated for the 2.x transition period.

`controls::DuiXmlBuilder` uses the same private parser to construct static control trees. It provides built-in `stack`, `vbox`, `hbox`, `grid`, `label`, `button`, `group-box`, `avatar`, `badge`, `separator`, `slider`, `switch`, `progress`, `status-bar`, `breadcrumb`, and `segmented` elements, and callers register custom controls and property appliers. Logical resource paths are resolved by a caller-provided UTF-8 callback; image decoding remains a platform capability.

Use `BuildWithResult` or `BuildFrameWithResult` in production code. Failures return a `DuiXmlError` containing a stable error code, UTF-8 message, element and attribute context, byte offset, and 1-based line and column. The pointer-only `Build` and optional-only `BuildFrame` entry points remain available for the 2.x transition period but are deprecated because they discard diagnostics.

`ysDui::resource` is an opt-in ZIP resource module. Configure with `-DYSDUI_BUILD_RESOURCE_MODULE=ON` to build `DuiResourcePackage` and `ysdui_resource_pack`; CMake fetches pinned `zlib-ng 2.2.5` and `minizip-ng 4.2.2`, with only Deflate enabled. The packer syntax is `ysdui_resource_pack output.zip entry=source...`. Set `-DYSDUI_ENABLE_RESOURCE_ENCRYPTION=ON` to add optional libsodium support. It requires a CMake package target such as vcpkg's `unofficial-sodium::sodium`; `--key-file=key.bin` accepts exactly 32 raw bytes and encrypts each ZIP entry with XChaCha20-Poly1305. Encrypted entries use ZIP Store because ciphertext is not compressible; unencrypted entries continue using Deflate. `DuiResourcePackage::Open` accepts the same `DuiResourceKey`; entry names remain visible for ZIP indexing, while content is authenticated against its entry name. When enabled, the install package exports `ysDui::resource`, the pinned dependency packages, the packer, and their license files.

The Gallery's Font Awesome Free 6.7.2 solid font is an external, Gallery-only asset. It is copied beside the Gallery executable and loaded at runtime; it is not embedded in the SDK or installed with `ysDui`. Its SHA-256 is `AF19D135D3A935B3EBFBD80320716FFE1202052C5F68DC2C5F1ABC57005AC605`, and its local license is `examples/DuiGallery/fonts/LICENSE.txt`.

Optional native capabilities are injected through narrow platform interfaces. `DuiTextInput` supplies native IME-backed editing to `DuiEditHost`; its Win32 `EDIT` remains a nonvisual input proxy for focus, keyboard, clipboard, and caret hit testing, while Canvas draws the field text, caret, background, and border. `DuiNativeViewHost` supplies external native-window embedding to `DuiNativeHost`. Both controls retain platform-neutral state, layout, and painting contracts, while the Win32 implementations privately own child-window creation and message handling.

Window-host capabilities are created through `IUiHostFactory`. `IEmbeddedHost`
accepts only portable content, dismissal policy, and `Rect` bounds; each backend
selects child or owned-popup behavior from the supplied anchor without exposing
that choice to controls. `IPopupHost`, `ILayeredHost`, and `IFrameHost` likewise
keep placement, composition, and frame options free of native window types.

`ui::HostRef` and `ui::NativeViewRef` are copyable weak references. They do not
extend a host or native view lifetime; a default reference is empty and a
reference becomes invalid after its owner closes or is destroyed. Controls use
these values rather than integer window handles. `NativeWindowHandle` remains
available only in explicit `platform_win32` interoperability APIs.

## Build And Test

Build the complete Windows configuration from a Visual Studio developer shell:

```powershell
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

The same configuration builds `ysdui_gallery`, a v2 Win32 control browser that composes platform-neutral controls through `core::Host` and renders them through the Win32 Canvas backend. The Gallery provides a searchable category tree and an interactive page for each public control. Run `ysdui_gallery --run-tests` for its headless control catalog regression suite. Run `ysdui_gallery --capture-all captures` to render every dedicated Gallery page into PNG fixtures without opening a visible window; the Win32-only capture path emits `gallery-<page>.png` files. Set `-DYSDUI_BUILD_EXAMPLES=OFF` to omit examples from a library-only build.

`ysdui_gallery_capture_all` runs that capture path in CTest and verifies all 69 output names and SHA-256 values against `tests/gallery_golden_sha256.txt`. The baseline is intentionally strict for the fixed Windows GDI+/DirectWrite offscreen renderer: update it only after reviewing an intended visual change against the `d1ab3f3` source baseline.

The Dialog page restores the source Gallery's collapsible modal, maximizable modal, no-button, and modeless variants. `DuiDialog` renders its title bar and button bar through portable controls, reports results asynchronously, and uses `DuiWin32ModalDialog::Wait` only when a Windows caller explicitly needs a synchronous adapter.

Verify core, render, and controls without the Windows backend:

```powershell
cmake -S . -B build-core -G "NMake Makefiles" -DYSDUI_BUILD_WIN32_BACKEND=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Build the optional resource module in a separate configuration:

```powershell
cmake -S . -B build-resource -G "NMake Makefiles" -DYSDUI_BUILD_WIN32_BACKEND=OFF -DYSDUI_BUILD_RESOURCE_MODULE=ON
cmake --build build-resource --target ysdui_resource_pack ysdui_resource_package_tests
ctest --test-dir build-resource --output-on-failure
```

The GitHub Actions resource jobs exercise both the unencrypted ZIP/Deflate path and the libsodium encryption path. Encryption remains opt-in for consumers because it requires an externally supplied libsodium package.

## CMake Presets

`CMakePresets.json` provides the reproducible configurations used by the project: `portable-gcc`, `portable-clang`, `windows-msvc`, and `resource`.

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Run the MSVC preset from a Visual Studio developer shell. The portable and resource presets use Ninja and select GCC/Clang where named.

The `public_header_audit` test compiles every public header independently and rejects Windows SDK and forbidden native types.

## Install And Consume

The current delivery form is static libraries. The install package exports
`ysDui::core`, `ysDui::render`, `ysDui::ui`, and `ysDui::controls`; a Windows
configuration also exports `ysDui::platform_win32`. When the resource option is
enabled, it additionally exports `ysDui::resource` and installs its pinned ZIP
dependencies as CMake packages.

```powershell
cmake -S . -B build-install -DYSDUI_BUILD_TESTS=OFF
cmake --build build-install
cmake --install build-install --prefix C:\ysDui
```

An installed consumer uses CMake config mode and requests only the components it
links:

```cmake
find_package(ysDui CONFIG REQUIRED COMPONENTS core render ui controls)
target_link_libraries(my_app PRIVATE ysDui::controls)
```

On Windows, add `platform_win32` to the requested components and link
`ysDui::platform_win32` when the application creates a Win32 host. Exported
MSVC targets propagate `/utf-8` because the public headers use UTF-8 comments.
Request `resource` when that option is enabled and link `ysDui::resource` to
consume ZIP packages from an installed tree. The install also carries the
dependency documentation and third-party license files. Font Awesome is not
part of this SDK install; deploy the Gallery font separately when using the
Gallery example.

## Backend Extension Points

Implement `render::Canvas` for another graphics backend. The current protocol covers clipping, rectangles, rounded rectangles, linear and radial gradients, ellipses, arcs, paths, text, images, and image clipping. A new platform host translates native input into `core::Event` and invokes a Canvas-backed paint handler.

The Windows backend currently uses DirectWrite for text, GDI for rectangular clipping, and GDI+ for vector drawing, image drawing, and image decoding. It is intentionally replaceable with another Windows graphics implementation without changing public headers.

`DuiTextInputOptions` carries multiline, wrapping, password, read-only, and maximum-length settings without exposing native editor styles. `DuiNativeViewOptions` explicitly records ownership and decoration handling when attaching an external native window. `platform::win32::DuiFrameChrome` contains the Windows 10/11 frame-corner adaptation and accepts only `NativeWindowHandle`.

`DuiRichTextInput` adds platform-neutral UTF-16 selection, character formatting, link activation, image attachment, quote-block, and file-card operations. `DuiRichEditHost` remains a core control, while the Windows backend privately maps these operations to `RICHEDIT50W` and OLE image objects. `DuiRichDocumentJson` persists the portable `DuiRichDocument` value model as UTF-8 JSON; text formatting, links, quotes, file cards, and image resource-package entry references are represented without RTF or OLE types.

## Current Limits

- Image decoding is synchronous and platform-provided.
- Golden PNG persistence is currently implemented by the Win32 backend; an offscreen Canvas capture adapter is still pending.
- Rich document JSON references image entries but does not embed binary image payloads. Resource packages use caller-provided raw 32-byte keys; password KDF, key rotation, and encrypted ZIP metadata remain outside the current protocol.
- Win32 drag-and-drop converts supported native file and bitmap data into `DuiDropPayload`; UI Automation exposes the control tree through private Win32 Fragment providers.
- v2 intentionally uses a breaking API. Future capability additions must preserve the documented module boundaries and platform-neutral public contracts.
