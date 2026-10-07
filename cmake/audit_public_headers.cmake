file(GLOB_RECURSE headers "${ROOT}/*.hpp")
set(forbidden "windows[.]h|windef[.]h|winuser[.]h|commctrl[.]h|atl|ATL|wtl|WTL|OLE|CCom|BSTR|VARIANT|Co[A-Z]|RECT|POINT|SIZE|COLORREF|HWND|HDC|WPARAM|LPARAM|LRESULT|HRESULT|IUnknown|IDispatch|IStream|IPicture")

# 扫描前剥离 C/C++ 注释。注释属于说明文字而非依赖：
# 例如控件源码里写「与 DuiWin32TextRenderer 一致」会被裸正则误判为越层引用。
# 假阳性会让守卫逐渐失去约束力，因此必须先剥离注释再匹配。
# 注意：
# - CMake 正则不支持非贪婪量词，块注释用 ([^*]|[*]+[^*/])* 形式规避；
# - 本仓 `//` 注释内不含 `/*`，故先剥离块注释、再剥离行注释即可；
# - 块注释替换为空格而非空串，避免 `Dui/*x*/Win32` 被拼成 `DuiWin32` 造成新的假阳性；
# - 不剥离字符串字面量，保证 `#include "windows.h"` 这类真实违规仍能被捕获。
macro(ysdui_strip_comments var)
  string(REGEX REPLACE "/[*]([^*]|[*]+[^*/])*[*]/" " " ${var} "${${var}}")
  string(REGEX REPLACE "//[^\n]*" "" ${var} "${${var}}")
endmacro()
foreach(header IN LISTS headers)
  get_filename_component(name "${header}" NAME)
  if(NOT name MATCHES "^Dui.*[.]hpp$")
    message(FATAL_ERROR "Public header must start with Dui: ${header}")
  endif()
  file(READ "${header}" content)
  ysdui_strip_comments(content)
  string(REGEX MATCH "${forbidden}" match "${content}")
  if(match)
    message(FATAL_ERROR "Public header leaks platform dependency: ${header} (${match})")
  endif()
endforeach()

if(DEFINED SOURCE_ROOT)
  file(GLOB_RECURSE core_sources
    "${ROOT}/core/*.hpp"
    "${SOURCE_ROOT}/core/*.cpp"
    "${SOURCE_ROOT}/core/*.hpp")
  foreach(source IN LISTS core_sources)
    file(READ "${source}" content)
    ysdui_strip_comments(content)
    string(REGEX MATCH "ysDui/render|render::|DuiRenderable|Canvas|platform::win32|platform/win32|DuiWin32" match "${content}")
    if(match)
      message(FATAL_ERROR "Core source violates the module boundary: ${source} (${match})")
    endif()
  endforeach()

  file(GLOB_RECURSE render_sources
    "${ROOT}/render/*.hpp"
    "${SOURCE_ROOT}/render/*.cpp"
    "${SOURCE_ROOT}/render/*.hpp")
  foreach(source IN LISTS render_sources)
    file(READ "${source}" content)
    ysdui_strip_comments(content)
    string(REGEX MATCH "ysDui/(ui|controls|platform)|NativeWindowHandle|platform::|DuiWin32" match "${content}")
    if(match)
      message(FATAL_ERROR "Render source violates the module boundary: ${source} (${match})")
    endif()
  endforeach()

  file(GLOB_RECURSE ui_sources
    "${ROOT}/ui/*.hpp"
    "${SOURCE_ROOT}/ui/*.cpp"
    "${SOURCE_ROOT}/ui/*.hpp")
  foreach(source IN LISTS ui_sources)
    file(READ "${source}" content)
    ysdui_strip_comments(content)
    string(REGEX MATCH "ysDui/(controls|platform)|NativeWindowHandle|platform::|DuiWin32" match "${content}")
    if(match)
      message(FATAL_ERROR "UI source violates the module boundary: ${source} (${match})")
    endif()
  endforeach()

  file(GLOB_RECURSE control_sources
    "${ROOT}/controls/*.hpp"
    "${SOURCE_ROOT}/controls/*.cpp"
    "${SOURCE_ROOT}/controls/*.hpp")
  foreach(source IN LISTS control_sources)
    file(READ "${source}" content)
    ysdui_strip_comments(content)
    string(REGEX MATCH "ysDui/platform|NativeWindowHandle|platform::win32|DuiWin32" match "${content}")
    if(match)
      message(FATAL_ERROR "Controls source violates the module boundary: ${source} (${match})")
    endif()
  endforeach()

  file(GLOB_RECURSE resource_sources
    "${ROOT}/resource/*.hpp"
    "${SOURCE_ROOT}/resource/*.cpp"
    "${SOURCE_ROOT}/resource/*.hpp")
  foreach(source IN LISTS resource_sources)
    file(READ "${source}" content)
    ysdui_strip_comments(content)
    string(REGEX MATCH "ysDui/(render|ui|controls|platform)|NativeWindowHandle|platform::|DuiWin32|windows[.]h|windef[.]h|winuser[.]h|commctrl[.]h|atl|ATL|wtl|WTL|OLE|CCom|BSTR|VARIANT|RECT|POINT|COLORREF|HWND|HDC|WPARAM|LPARAM|LRESULT|HRESULT|IUnknown|IDispatch|IStream|IPicture" match "${content}")
    if(match)
      message(FATAL_ERROR "Resource source violates the module boundary: ${source} (${match})")
    endif()
  endforeach()
endif()
