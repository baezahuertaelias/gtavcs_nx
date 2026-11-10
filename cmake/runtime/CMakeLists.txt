cmake_minimum_required(VERSION 3.24)
option(NX_NATIVE_REFERENCE "Build the isolated x86 native test counterpart" OFF)
if(NOT CMAKE_TOOLCHAIN_FILE AND NOT NX_NATIVE_REFERENCE)
    message(FATAL_ERROR "Use the official devkitPro Switch.cmake toolchain")
endif()

project(gtavcs_runtime_foundation LANGUAGES CXX)
if(NOT NINTENDO_SWITCH AND NOT NX_NATIVE_REFERENCE)
    message(FATAL_ERROR "Switch-only compile project")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${PORT_ROOT}/cmake/CPUOptions.cmake")
function(runtime_cpu_options target)
    if(NINTENDO_SWITCH)
        nx_cpu_options(${target})
        target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
    else()
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "Native reference requires the recorded Clang compiler")
        endif()
        target_compile_options(${target} PRIVATE -O2 -march=x86-64-v3
            -ffp-contract=off -frounding-math -fno-strict-aliasing -fsigned-char)
        # Upstream's public test callbacks omit PS2_GUEST_CC annotations. Use
        # its documented platform-ABI test configuration consistently across
        # the native runtime/callers; never cast mismatched function pointers.
        target_compile_definitions(${target} PRIVATE PS2X_GUEST_CC_DEFAULT=1)
        target_include_directories(${target} PRIVATE
            "${runtime}/include" "${runtime}/src/lib/Kernel"
            "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xIOP/include")
    endif()
endfunction()
set(runtime "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRuntime")
# Explicit library inventory from the pinned runtime CMakeLists, including CPU
# GS and unavailable-GPU selector. No runner, desktop backends, JIT or FFmpeg.
# This project is compile-only until the null host boundary is implemented.
set(runtime_sources
    game_overrides.cpp gs/ps2_gif_arbiter.cpp ps2_audio.cpp ps2_audio_vag.cpp
    gs/ps2_gs_memory.cpp gs/gs_frontend.cpp gs/gs_cpu_backend.cpp
    ps2_iop_host.cpp ps2_memory.cpp ps2_pad.cpp ps2_rom_device.cpp ps2_vfs.cpp
    ps2_disc_image.cpp ps2_audio_out.cpp ps2_perf.cpp ps2_control.cpp
    ps2_hle_survey.cpp ps2_runtime.cpp ps2_vif1_interpreter.cpp
    ps2_render_worker.cpp ps2_write_watch.cpp ps2_host_std.cpp ps2_iop_native.cpp
    vu/ps2_vu1_core.cpp vu/ps2_vu1_upper.cpp vu/ps2_vu1_lower.cpp
    vu/ps2_vu_code_log.cpp games_database.cpp
    gs/gs_gpu.cpp gs/gs_hw.cpp gs/gs_gpu_none.cpp)
list(TRANSFORM runtime_sources PREPEND "${runtime}/src/lib/")
file(GLOB_RECURSE kernel_sources CONFIGURE_DEPENDS "${runtime}/src/lib/Kernel/*.cpp")
add_library(nx_ps2_runtime STATIC ${runtime_sources} ${kernel_sources}
    "${VCS_UPSTREAM_DIR}/src/debug/watch.cpp")
runtime_cpu_options(nx_ps2_runtime)
target_include_directories(nx_ps2_runtime PRIVATE "${VCS_UPSTREAM_DIR}/src")
target_compile_definitions(nx_ps2_runtime PRIVATE PS2X_HAS_FFMPEG=0
    PS2X_ENABLE_IOP_RPC_TRACE=0 PS2X_HEADLESS=1 _POSIX_C_SOURCE=200809L)
set(PS2X_IOP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xIOP" iop)
runtime_cpu_options(ps2_iop)
target_link_libraries(nx_ps2_runtime PUBLIC ps2_iop)
add_library(nx_cpu_host STATIC "${PORT_ROOT}/src/core_tests/null_platform.cpp"
    "${PORT_ROOT}/src/core_tests/null_presenter.cpp"
    "${VCS_UPSTREAM_DIR}/src/rhi/rhi.cpp"
    "${VCS_UPSTREAM_DIR}/src/rhi/null/rhi_null.cpp")
runtime_cpu_options(nx_cpu_host)
target_include_directories(nx_cpu_host PUBLIC "${VCS_UPSTREAM_DIR}/src")
target_compile_definitions(nx_cpu_host PRIVATE PS2X_HEADLESS=1)
target_link_libraries(nx_ps2_runtime PUBLIC nx_cpu_host)
# Diagnostic counterpart against the pristine pin: it has unconditional ImGui
# includes in perf/null RHI. No platform backend or UI context is started.
set(NX_BASELINE_IMGUI_DIR "" CACHE PATH "Public ImGui sources for pristine native comparison only")
if(NX_BASELINE_IMGUI_DIR)
    if(NINTENDO_SWITCH)
        message(FATAL_ERROR "Pristine comparison is native-only")
    endif()
    add_library(nx_baseline_imgui STATIC
        "${NX_BASELINE_IMGUI_DIR}/imgui.cpp" "${NX_BASELINE_IMGUI_DIR}/imgui_draw.cpp"
        "${NX_BASELINE_IMGUI_DIR}/imgui_tables.cpp" "${NX_BASELINE_IMGUI_DIR}/imgui_widgets.cpp")
    target_include_directories(nx_baseline_imgui PUBLIC "${NX_BASELINE_IMGUI_DIR}")
    target_link_libraries(nx_cpu_host PUBLIC nx_baseline_imgui)
    target_link_libraries(nx_ps2_runtime PRIVATE nx_baseline_imgui)
endif()

option(NX_RUNTIME_TESTS "Build the public upstream runtime suites" ON)
if(NX_RUNTIME_TESTS)
    set(test_root "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xTest")
    set(test_sources pad_input_tests.cpp ps2_runtime_io_tests.cpp
        ps2_runtime_kernel_tests.cpp ps2_runtime_interrupt_tests.cpp
        ps2_memory_tests.cpp ps2_vu1_tests.cpp ps2_vu_tests.cpp ps2_gs_tests.cpp
        ps2_iop_tests.cpp ps2_sif_rpc_tests.cpp ps2_sif_dma_tests.cpp
        ps2_runtime_expansion_tests.cpp ps2_float_semantics_tests.cpp
        test_function_table.cpp)
    list(TRANSFORM test_sources PREPEND "${test_root}/src/")
    add_executable(gtavcs_runtime_tests ${test_sources}
        "${PORT_ROOT}/tests/core/scheduler_workload.cpp"
        "${PORT_ROOT}/tests/core/guest_dispatch_tests.cpp"
        "${PORT_ROOT}/tests/core/compiled_vu_tests.cpp"
        "${PORT_ROOT}/tests/core/iop_directory_bounds_tests.cpp"
        "${PORT_ROOT}/tests/core/folder_disc_tests.cpp"
        "${PORT_ROOT}/src/vfs/folder_disc.cpp"
        "${PORT_ROOT}/src/core_tests/runtime_main.cpp")
    runtime_cpu_options(gtavcs_runtime_tests)
    target_include_directories(gtavcs_runtime_tests PRIVATE "${test_root}/include"
        "${runtime}/src/lib" "${test_root}/src"
        "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRecomp/include")
    target_compile_definitions(gtavcs_runtime_tests PRIVATE NX_NO_GENERATOR_TESTS)
    option(NX_EXTENDED_CPU_TESTS "Include the port's scalar/differential boundary suites" ON)
    if(NX_EXTENDED_CPU_TESTS)
        target_sources(gtavcs_runtime_tests PRIVATE
            "${PORT_ROOT}/tests/core/packed_integer_tests.cpp"
            "${PORT_ROOT}/tests/core/float_tests.cpp"
            "${PORT_ROOT}/tests/core/host_boundary_tests.cpp")
        target_compile_definitions(gtavcs_runtime_tests PRIVATE
            NX_CORE_TEST_SUITE NX_EXTENDED_CPU_TESTS NX_PMFHL_ARCHITECTURAL)
        if(NINTENDO_SWITCH)
            target_sources(gtavcs_runtime_tests PRIVATE "${PORT_ROOT}/tests/core/switch_alloc_tests.cpp")
        endif()
    endif()
    target_link_libraries(gtavcs_runtime_tests PRIVATE nx_ps2_runtime)
    if(NINTENDO_SWITCH)
        target_link_options(gtavcs_runtime_tests PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/gtavcs_runtime_tests.map")
    endif()
    if(NINTENDO_SWITCH)
        target_sources(gtavcs_runtime_tests PRIVATE "${PORT_ROOT}/src/core_tests/heap.cpp")
        target_link_libraries(gtavcs_runtime_tests PRIVATE nx m)
        set_target_properties(gtavcs_runtime_tests PROPERTIES SUFFIX ".elf")
        nx_generate_nacp(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_runtime_tests.nacp"
            NAME "GTA VCS NX - Runtime tests" AUTHOR "gtavcs_nx contributors" VERSION "0.2.0")
        nx_create_nro(gtavcs_runtime_tests OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_runtime_tests.nro"
            NACP "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_runtime_tests.nacp")
    elseif(MINGW)
        target_link_libraries(nx_ps2_runtime PUBLIC ws2_32)
        target_link_options(gtavcs_runtime_tests PRIVATE -static -Wl,--stack,8388608)
    endif()
endif()

set(NX_PRIVATE_GENERATED_ARCHIVE "" CACHE FILEPATH "Private EE/VU archive; never used by source CI")
if(NX_PRIVATE_GENERATED_ARCHIVE)
    if(NOT NINTENDO_SWITCH OR NOT EXISTS "${NX_PRIVATE_GENERATED_ARCHIVE}")
        message(FATAL_ERROR "Private core linking requires an existing Switch archive")
    endif()
    add_executable(gtavcs_private_core "${PORT_ROOT}/src/core_tests/generated_main.cpp"
        "${PORT_ROOT}/tests/core/scheduler_workload.cpp"
        "${PORT_ROOT}/tests/core/guest_dispatch_tests.cpp"
        "${PORT_ROOT}/src/core_tests/heap.cpp")
    target_sources(gtavcs_private_core PRIVATE "${PORT_ROOT}/tests/core/iop_directory_bounds_tests.cpp")
    runtime_cpu_options(gtavcs_private_core)
    target_link_libraries(gtavcs_private_core PRIVATE
        -Wl,--whole-archive "${NX_PRIVATE_GENERATED_ARCHIVE}" -Wl,--no-whole-archive
        nx_ps2_runtime nx m)
    target_link_options(gtavcs_private_core PRIVATE
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/gtavcs_private_core.map")
    set_target_properties(gtavcs_private_core PROPERTIES SUFFIX ".elf")
    nx_generate_nacp(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_private_core.nacp"
        NAME "GTA VCS NX - Private CPU core" AUTHOR "gtavcs_nx contributors" VERSION "0.2.0")
    nx_create_nro(gtavcs_private_core OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_private_core.nro"
        NACP "${CMAKE_CURRENT_BINARY_DIR}/gtavcs_private_core.nacp")
endif()
