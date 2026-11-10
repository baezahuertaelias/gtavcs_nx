cmake_minimum_required(VERSION 3.24)
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Use the official devkitPro Switch.cmake toolchain")
endif()
project(gtavcs_cpu_tests VERSION 0.2.0 LANGUAGES CXX)
if(NOT NINTENDO_SWITCH OR NOT COMMAND nx_create_nro)
    message(FATAL_ERROR "Official Switch toolchain and packaging helpers required")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${PORT_ROOT}/cmake/CPUOptions.cmake")
add_executable(gtavcs_cpu_tests
    "${PORT_ROOT}/src/core_tests/main.cpp"
    "${PORT_ROOT}/src/core_tests/heap.cpp"
    "${PORT_ROOT}/tests/core/switch_alloc_tests.cpp"
    "${PORT_ROOT}/tests/core/packed_integer_tests.cpp"
    "${PORT_ROOT}/tests/core/host_boundary_tests.cpp"
    "${PORT_ROOT}/tests/core/float_tests.cpp")
set_target_properties(gtavcs_cpu_tests PROPERTIES SUFFIX ".elf")
nx_cpu_options(gtavcs_cpu_tests)
target_include_directories(gtavcs_cpu_tests PRIVATE
    "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xTest/src")
target_compile_definitions(gtavcs_cpu_tests PRIVATE NX_CORE_TEST_SUITE)
target_link_options(gtavcs_cpu_tests PRIVATE
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/gtavcs_cpu_tests.map" -Wl,--gc-sections)
target_link_libraries(gtavcs_cpu_tests PRIVATE nx m)
nx_generate_nacp(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_cpu_tests.nacp"
    NAME "GTA VCS NX - CPU tests" AUTHOR "gtavcs_nx contributors" VERSION "${PROJECT_VERSION}")
nx_create_nro(gtavcs_cpu_tests OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_cpu_tests.nro"
    NACP "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_cpu_tests.nacp")
