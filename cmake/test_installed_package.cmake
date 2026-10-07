# test_installed_package.cmake
# 开发者：青蓝
# 开发时间：2026-07-29
# 用途：安装当前构建并用独立消费者验证 find_package 交付。

foreach(required IN ITEMS YSDUI_SOURCE_DIR YSDUI_BINARY_DIR YSDUI_GENERATOR YSDUI_CTEST_COMMAND)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "Missing required variable: ${required}")
  endif()
endforeach()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef install_suffix)
set(install_dir "${YSDUI_BINARY_DIR}/install-consumer-prefix-${install_suffix}")
set(consumer_build_dir "${YSDUI_BINARY_DIR}/install-consumer-build-${install_suffix}")
if(YSDUI_CXX_COMPILER)
  string(SHA256 compiler_hash "${YSDUI_CXX_COMPILER}")
  string(SUBSTRING "${compiler_hash}" 0 12 compiler_hash)
  set(consumer_build_dir "${consumer_build_dir}-${compiler_hash}")
endif()

set(install_command "${CMAKE_COMMAND}" --install "${YSDUI_BINARY_DIR}" --prefix "${install_dir}")
if(YSDUI_TEST_CONFIG)
  list(APPEND install_command --config "${YSDUI_TEST_CONFIG}")
endif()
execute_process(COMMAND ${install_command} RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "ysDui installation failed: ${install_result}")
endif()

foreach(required_file IN ITEMS
    "${install_dir}/include/ysDui/core/DuiVersion.hpp"
    "${install_dir}/lib/cmake/ysDui/ysDuiConfig.cmake"
    "${install_dir}/lib/cmake/ysDui/ysDuiTargets.cmake"
    "${install_dir}/share/ysDui/DEPENDENCIES.md"
    "${install_dir}/share/ysDui/RELEASE.md"
    "${install_dir}/share/ysDui/licenses/nanosvg-LICENSE.txt"
    "${install_dir}/share/ysDui/licenses/pugixml-LICENSE.md")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "ysDui installation is missing required file: ${required_file}")
  endif()
endforeach()
if(YSDUI_INSTALL_HAS_RESOURCE)
  if(WIN32)
    set(resource_packer_suffix ".exe")
  else()
    set(resource_packer_suffix "")
  endif()
  foreach(resource_file IN ITEMS
      "${install_dir}/include/ysDui/resource/DuiResourcePackage.hpp"
      "${install_dir}/lib/cmake/minizip-ng/minizip-ng-config.cmake"
      "${install_dir}/lib/cmake/zlib-ng/zlib-ng-config.cmake"
      "${install_dir}/bin/ysdui_resource_pack${resource_packer_suffix}"
      "${install_dir}/share/ysDui/licenses/minizip-ng-LICENSE"
      "${install_dir}/share/ysDui/licenses/zlib-ng-LICENSE.md")
    if(NOT EXISTS "${resource_file}")
      message(FATAL_ERROR "ysDui resource installation is missing required file: ${resource_file}")
    endif()
  endforeach()
endif()
if(EXISTS "${YSDUI_SOURCE_DIR}/LICENSE"
    AND NOT EXISTS "${install_dir}/share/ysDui/LICENSE")
  message(FATAL_ERROR "ysDui installation is missing the project LICENSE")
endif()

set(configure_command
  "${CMAKE_COMMAND}"
  -S "${YSDUI_SOURCE_DIR}/tests/install_consumer"
  -B "${consumer_build_dir}"
  -G "${YSDUI_GENERATOR}"
  "-DYSDUI_INSTALL_HAS_WIN32=${YSDUI_INSTALL_HAS_WIN32}"
  "-DYSDUI_INSTALL_HAS_RESOURCE=${YSDUI_INSTALL_HAS_RESOURCE}"
  "-DCMAKE_PREFIX_PATH=${install_dir}")
if(YSDUI_GENERATOR_PLATFORM)
  list(APPEND configure_command -A "${YSDUI_GENERATOR_PLATFORM}")
endif()
if(YSDUI_CXX_COMPILER)
  list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${YSDUI_CXX_COMPILER}")
endif()
if(YSDUI_MAKE_PROGRAM)
  list(APPEND configure_command "-DCMAKE_MAKE_PROGRAM=${YSDUI_MAKE_PROGRAM}")
endif()
if(YSDUI_BUILD_TYPE)
  list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${YSDUI_BUILD_TYPE}")
endif()
# 加密资源包依赖 unofficial-sodium：消费者必须能从同一 vcpkg toolchain 找到它。
if(YSDUI_TOOLCHAIN_FILE)
  list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${YSDUI_TOOLCHAIN_FILE}")
endif()
if(YSDUI_VCPKG_TARGET_TRIPLET)
  list(APPEND configure_command "-DVCPKG_TARGET_TRIPLET=${YSDUI_VCPKG_TARGET_TRIPLET}")
endif()
execute_process(COMMAND ${configure_command} RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Installed ysDui consumer configuration failed: ${configure_result}")
endif()

set(build_command "${CMAKE_COMMAND}" --build "${consumer_build_dir}")
if(YSDUI_TEST_CONFIG)
  list(APPEND build_command --config "${YSDUI_TEST_CONFIG}")
endif()
execute_process(COMMAND ${build_command} RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Installed ysDui consumer build failed: ${build_result}")
endif()

set(test_command "${YSDUI_CTEST_COMMAND}" --test-dir "${consumer_build_dir}" --output-on-failure)
if(YSDUI_TEST_CONFIG)
  list(APPEND test_command -C "${YSDUI_TEST_CONFIG}")
endif()
execute_process(COMMAND ${test_command} RESULT_VARIABLE test_result)
if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "Installed ysDui consumer test failed: ${test_result}")
endif()
