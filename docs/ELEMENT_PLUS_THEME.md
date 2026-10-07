# Element Plus Theme Mapping

`ThemePreset::ElementLight` is an optional native ysDui theme inspired by the
Element Plus light palette. It does not link or execute Element Plus, Vue,
Node.js, WebView2, or a browser runtime. Existing `Light`, `Dark`, and
`HighContrast` presets remain unchanged.

## Core Mapping

| Element Plus reference | ysDui slot | Value |
| --- | --- | --- |
| Primary | `BrandPrimary` | `#409EFF` |
| Primary light/hover | `BrandHover` | `#66B1FF` |
| Primary dark/pressed | `BrandPressed` | `#3A8EE6` |
| Success | `StatusOnline` | `#67C23A` |
| Warning | `StatusAway`, `MessageWarningFill` | `#E6A23C` |
| Danger | `Danger`, `StatusBusy`, `MessageErrorFill` | `#F56C6C` |
| Info | `StatusOffline` | `#909399` |
| Primary text | `TextDefault`, `FieldText`, `ControlText` | `#303133` |
| Secondary text | `TextSubtle` | `#909399` |
| Placeholder | `FieldPlaceholder`, `SearchPlaceholder` | `#A8ABB2` |
| Disabled text | `TextDisabled` | `#C0C4CC` |
| Base border | `BorderHeavy`, `FieldBorder`, `PopupBorder` | `#DCDFE6` |
| Light border | `BorderLight`, `GridLine` | `#EBEEF5` |
| Selected fill | `SelectionBackground`, `GridSelection` | `#ECF5FF` |
| Secondary surface | `SurfaceAlternateBackground` | `#F5F7FA` |

Control-specific slots remain part of the existing platform-neutral theme API.
They are derived from these semantic colors and can still be overridden with
`DuiTheme::Set`. Icons continue to use ysDui's SVG, bitmap, and icon-font
pipelines; no Element Plus icon package is imported.

## Usage

```cpp
ysDui::core::Host host;
host.Theme().ApplyPreset(ysDui::core::ThemePreset::ElementLight);
```

Applying a preset changes colors only. The configured UTF-8 font family and
font point size are preserved.
