# ysDui v2 Architecture

## Module Boundaries

The static-library dependency graph is fixed:

```text
controls -> ui / render / core
ui       -> render / core
render   -> core
resource -> core
platform-win32 -> controls / ui / render / core
```

`core`, `render`, `ui`, and `controls` use only standard C++ and ysDui public
headers. They do not include Windows SDK, COM, OLE, ATL, WTL, Win32 handles, or
Win32 backend headers. `platform-win32` owns all Windows windows, messages,
GDI/GDI+, COM/OLE, UI Automation, and native controls.

The CMake targets are `ysDui::core`, `ysDui::render`, `ysDui::ui`, and
`ysDui::controls`; Windows builds additionally provide
`ysDui::platform_win32`. When enabled, the resource module exports
`ysDui::resource` together with the pinned minizip-ng/zlib-ng package contract.
Its public and private sources are audited to depend only on `core`; both
unencrypted and encrypted paths are covered by CI.

## Host And Native References

`ui::HostRef` and `ui::NativeViewRef` are portable, copyable weak references.
They never store a raw integer native handle and do not own their target. A
default reference is empty; an otherwise nonempty reference becomes invalid as
soon as the backend destroys its host or native view. Backends reject references
that are empty, invalid, or created by another backend.

`IFrameHost`, `IPopupHost`, `IEmbeddedHost`, and `ILayeredHost` expose
`Reference()` so controls and host factories exchange `HostRef`, not Child,
Popup, or Layered implementation details. `NativeWindowHandle` is limited to
explicit Win32 interoperability APIs; it cannot enter ordinary controls or the
`ui` layer.

## DIP And DPI

All `Point`, `Size`, `Rect`, layout bounds, hit-test positions, `Event`
positions, and Canvas coordinates are 96-DPI logical pixels (DIP). `core::Host`
owns the platform DPI scale and converts only at the Win32 boundary. Rectangle
edges are scaled and rounded independently so repeated size conversions do not
accumulate width or height errors.

For a Win32 DPI change, the backend updates the Host DPI value first, applies
the suggested physical window rectangle, relayouts the portable control tree,
repositions native child editors, and schedules one redraw. Controls must not
make their own physical-pixel conversions.

## Theme Context

Each `core::Host` owns a `DuiTheme`. Theme resolution is fixed as:

```text
explicit control theme > inherited Host theme > built-in default theme
```

Controls inherit the Host theme through the tree. A theme mutation increments
the theme version and invokes the Host theme-change callback so layout and paint
can be invalidated. There is no mutable global theme singleton. Gallery pages
only arrange and demonstrate controls; the default control appearance belongs
to the control layer and Light remains the BalloonUI `d1ab3f3` baseline.

## Rendering And Input

`render::Canvas` is a complete pure virtual protocol for clipping, rectangles,
rounded rectangles, gradients, ellipses, arcs, paths, images, text, and text
measurement. Implementations must provide every command and must not silently
fall back to byte-count text measurement. The Win32 backend uses GDI+ for
vector and image drawing and DirectWrite for shaping, bidirectional layout,
wrapping, font fallback, measurement, and text drawing.

`core::Event` carries UTF-8 text, key press/release, composition input,
pointer leave/cancel, pointer identity, device type, and DPI changes. The
Win32 event converter and native text adapter are backend-private. IME-backed
native editing remains an injected `ui` capability. The Win32 `EDIT` is a
nonvisual proxy for focus, keyboard, clipboard, IME, and caret hit testing;
Canvas controls own visible text, caret, background, and border rendering.

## Adding A Backend

1. Keep core, render, ui, and controls unchanged and free of the new platform
   SDK.
2. Implement every Canvas command and verify it against the shared Canvas mock
   command sequence.
3. Convert the platform event loop into `core::Event` DIP coordinates and own
   its `core::Host` DPI and theme contexts.
4. Implement the applicable `ui` capability interfaces and make `HostRef` and
   `NativeViewRef` reject foreign-backend values.
5. Add an optional CMake target that depends on the four portable targets, then
   run public-header, portable, and backend regression suites.

## Current Limits

- Only the Win32 window backend exists. Linux GCC and Clang builds demonstrate
  that portable modules compile without the Windows SDK; they do not provide a
  Linux window host.
- GDI+ remains the Win32 vector and image renderer. D2D and Skia are not part
  of this architecture stage.
- Delivery is static libraries only. DLL export macros and ABI stability are
  not promised.
- The ZIP resource package format is unchanged. Its optional encryption path
  requires an externally supplied libsodium package and is not enabled by
  default.
- Gallery visual regression is a strict 69-page SHA-256 baseline for the Win32
  GDI+/DirectWrite offscreen renderer. It protects deliberate Windows style changes, not
  cross-platform font rasterization equivalence.
