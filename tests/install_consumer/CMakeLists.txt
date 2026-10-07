# CMakeLists.txt
# 开发者：青蓝
# 开发时间：2026-07-29
# 用途：验证安装后的 ysDui 配置包、组件和导出目标。

cmake_minimum_required(VERSION 3.24)
project(ysDuiInstallConsumer LANGUAGES CXX)

set(ysdui_components core render ui controls workbook_text)
if(YSDUI_INSTALL_HAS_WIN32)
  list(APPEND ysdui_components platform_win32)
endif()
if(YSDUI_INSTALL_HAS_RESOURCE)
  list(APPEND ysdui_components resource)
endif()
find_package(ysDui 2.0 CONFIG REQUIRED COMPONENTS ${ysdui_components})

add_executable(ysdui_install_consumer main.cpp)
target_compile_features(ysdui_install_consumer PRIVATE cxx_std_20)
target_link_libraries(ysdui_install_consumer PRIVATE ysDui::controls ysDui::workbook_text)
if(YSDUI_INSTALL_HAS_WIN32)
  target_link_libraries(ysdui_install_consumer PRIVATE ysDui::platform_win32)
endif()
if(YSDUI_INSTALL_HAS_RESOURCE)
  target_link_libraries(ysdui_install_consumer PRIVATE ysDui::resource)
  target_compile_definitions(ysdui_install_consumer PRIVATE YSDUI_INSTALL_HAS_RESOURCE=1)
endif()

enable_testing()
add_test(NAME ysdui_installed_consumer_runs COMMAND ysdui_install_consumer)
