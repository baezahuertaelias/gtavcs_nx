# Shared Switch CPU-test/generated-code constraints. Call after project().
function(nx_cpu_options target)
    foreach(required VCS_UPSTREAM_DIR SSE2NEON_INCLUDE_DIR)
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
    target_include_directories(${target} PRIVATE
        "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRuntime/include"
        "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xRuntime/src/lib/Kernel"
        "${VCS_UPSTREAM_DIR}/third_party/PS2Recomp/ps2xIOP/include"
        "${SSE2NEON_INCLUDE_DIR}" "${PORT_ROOT}/src")
    target_compile_definitions(${target} PRIVATE USE_SSE2NEON
        SSE2NEON_PRECISE_MINMAX=1 SSE2NEON_PRECISE_DIV=1
        SSE2NEON_PRECISE_SQRT=1 SSE2NEON_PRECISE_DP=1)
    target_compile_options(${target} PRIVATE
        -O2 -fPIE -ffp-contract=off -frounding-math -fno-strict-aliasing -fsigned-char
        -include "${PORT_ROOT}/src/compat/switch_sse2neon_alloc.h")
endfunction()
