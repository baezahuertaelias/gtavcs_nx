cmake_minimum_required(VERSION 3.24)
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Use the official devkitPro Switch.cmake toolchain")
endif()
project(gtavcs_cpu_foundation LANGUAGES CXX)
if(NOT NINTENDO_SWITCH)
    message(FATAL_ERROR "This project compiles the private ARM64 foundation only")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
foreach(required VCS_UPSTREAM_DIR VCS_GENERATED_DIR SSE2NEON_INCLUDE_DIR)
    if(NOT IS_DIRECTORY "${${required}}")
        message(FATAL_ERROR "Provide ${required} explicitly")
    endif()
endforeach()
file(READ "${PORT_ROOT}/tools/core-dependencies.json" dependency_lock)
string(JSON expected_header GET "${dependency_lock}" sse2neon header_sha256)
file(SHA256 "${SSE2NEON_INCLUDE_DIR}/sse2neon.h" actual_header)
if(NOT actual_header STREQUAL expected_header)
    message(FATAL_ERROR "SSE2NEON content does not match the locked dependency")
endif()
file(READ "${PORT_ROOT}/patches/upstream/series.json" source_lock)
string(JSON expected_commit GET "${source_lock}" base_commit)
file(READ "${VCS_GENERATED_DIR}/recompile_stamp.json" generation_stamp)
string(JSON generated_commit GET "${generation_stamp}" source commit)
if(NOT generated_commit STREQUAL expected_commit)
    message(FATAL_ERROR "Private code was not generated from the pinned source")
endif()
file(GLOB ee_sources CONFIGURE_DEPENDS "${VCS_GENERATED_DIR}/ee/*.cpp")
if(NOT EXISTS "${VCS_GENERATED_DIR}/ee/register_functions.cpp" OR NOT ee_sources)
    message(FATAL_ERROR "Missing private generated EE code")
endif()
# Compile-only foundation. This does not link a runtime, boot a game, or replace
# the root platform smoke NRO. Core harness/runtime linkage is a separate task.
add_library(vcs_generated_cpu STATIC ${ee_sources}
    "${VCS_GENERATED_DIR}/vu1/vu1_compiled.cpp")
# The registration constructor has 236k assignments and VU output has 110k
# lines. Do not combine both into one optimization unit. Upstream already keeps
# VU code out of EE unity batches; retain full O2 but omit unused DWARF here.
set_source_files_properties("${VCS_GENERATED_DIR}/ee/register_functions.cpp"
    "${VCS_GENERATED_DIR}/vu1/vu1_compiled.cpp" PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION TRUE COMPILE_OPTIONS "-g0")
set_target_properties(vcs_generated_cpu PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 32
    POSITION_INDEPENDENT_CODE OFF)
target_include_directories(vcs_generated_cpu PRIVATE
    "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRuntime/include"
    "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRuntime/src/lib/Kernel"
    "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xIOP/include"
    "${VCS_GENERATED_DIR}/ee" "${SSE2NEON_INCLUDE_DIR}")
target_compile_definitions(vcs_generated_cpu PRIVATE USE_SSE2NEON
    SSE2NEON_PRECISE_MINMAX=1 SSE2NEON_PRECISE_DIV=1
    SSE2NEON_PRECISE_SQRT=1 SSE2NEON_PRECISE_DP=1)
target_compile_options(vcs_generated_cpu PRIVATE
    -O2 -fPIE -ffp-contract=off -frounding-math -fno-strict-aliasing -fsigned-char
    -include "${PORT_ROOT}/src/compat/switch_sse2neon_alloc.h")
