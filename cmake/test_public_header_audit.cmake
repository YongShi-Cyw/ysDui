# 文件名：test_public_header_audit.cmake
# 开发者：青蓝
# 开发时间：2026-10-05
# 用途：自检 audit_public_headers.cmake——既不能放行真实越界依赖（有效），
#       也不能因注释中的禁用名而误报（不误报）。防止该守卫退化为空转。
#
# 说明：此前的误报形态是注释里出现 `DuiWin32`，因此这里专门构造
#       「禁用名只出现在注释中」的用例，确保剥离注释逻辑不被回退。

if(NOT DEFINED AUDIT_SCRIPT)
  message(FATAL_ERROR "test_public_header_audit: 必须指定 AUDIT_SCRIPT")
endif()
if(NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "test_public_header_audit: 必须指定 WORK_DIR")
endif()

# 断言某个用例的审计结果。
# @param case_name 用例目录名（位于 WORK_DIR 下）
# @param expected_pass true 期望通过；false 期望被拒绝
function(expect_audit case_name expected_pass)
  set(case_dir "${WORK_DIR}/${case_name}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DROOT=${case_dir}/include" "-DSOURCE_ROOT=${case_dir}/src"
            -P "${AUDIT_SCRIPT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(expected_pass)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR
              "审计自检失败：用例 ${case_name} 期望通过，实际被拒绝（rc=${result}）\n${output}\n${error}")
    endif()
  else()
    if(result EQUAL 0)
      message(FATAL_ERROR "审计自检失败：用例 ${case_name} 期望被拒绝，实际通过")
    endif()
  endif()
endfunction()

# 写入一个用例的公开头与 controls 源文件（重复写入即覆盖，无需清理旧目录）。
# @param case_name 用例目录名
# @param header_body 公开头内容（文件名固定为 DuiAuditFixture.hpp）
# @param source_body controls 源内容（文件名固定为 DuiAuditFixture.cpp）
function(write_fixture case_name header_body source_body)
  set(case_dir "${WORK_DIR}/${case_name}")
  file(MAKE_DIRECTORY "${case_dir}/include" "${case_dir}/src/controls")
  file(WRITE "${case_dir}/include/DuiAuditFixture.hpp" "${header_body}")
  file(WRITE "${case_dir}/src/controls/DuiAuditFixture.cpp" "${source_body}")
endfunction()

# 公开头必须满足「以 Dui 开头」的命名约定，故各用例共用此前缀。
set(header_prefix "#pragma once\n\n")

# 用例 1：禁用名只出现在行注释中 —— 必须放行（即此前的真实误报形态）。
write_fixture(comment_line
  "${header_prefix}// 说明：本文件不使用 HWND / RECT / POINT 等平台类型。\nclass DuiAuditFixture {};\n"
  "#include \"DuiAuditFixture.hpp\"\n\n// 与 DuiWin32TextRenderer 一致：fontSizeDip = pointSize * 96/72。\nint DuiAuditFixtureValue() { return 1; }\n")
expect_audit(comment_line TRUE)

# 用例 2：禁用名只出现在块注释中 —— 必须放行（覆盖块注释剥离分支）。
write_fixture(comment_block
  "${header_prefix}/**\n * 块注释说明：本文件不使用 HWND / COLORREF。\n */\nclass DuiAuditFixture {};\n"
  "#include \"DuiAuditFixture.hpp\"\n\n/* DuiWin32 仅出现在块注释中 */\nint DuiAuditFixtureValue() { return 1; }\n")
expect_audit(comment_block TRUE)

# 用例 3：真实包含 Windows SDK 头 —— 必须被拒绝。
write_fixture(include_windows
  "${header_prefix}#include <windows.h>\nclass DuiAuditFixture {};\n"
  "#include \"DuiAuditFixture.hpp\"\n\nint DuiAuditFixtureValue() { return 1; }\n")
expect_audit(include_windows FALSE)

# 用例 4：代码中真实使用平台类型 —— 必须被拒绝。
write_fixture(type_hwnd
  "${header_prefix}class DuiAuditFixture\n{\n    HWND handle_ = nullptr;\n};\n"
  "#include \"DuiAuditFixture.hpp\"\n\nint DuiAuditFixtureValue() { return 1; }\n")
expect_audit(type_hwnd FALSE)

# 用例 5：controls 源码真实引用平台后端 —— 必须被拒绝。
write_fixture(controls_platform
  "${header_prefix}class DuiAuditFixture {};\n"
  "#include \"DuiAuditFixture.hpp\"\n\nint DuiAuditFixtureValue() { return DuiWin32TextRendererValue(); }\n")
expect_audit(controls_platform FALSE)

message(STATUS "audit_public_headers 自检通过：2 例放行 / 3 例拒绝均符合预期")
