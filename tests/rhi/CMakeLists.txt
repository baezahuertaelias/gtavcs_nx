# Host build of the RHI conformance scenes (tools/rhi_host_tests.py drives it): upstream's Vulkan backend (the desktop
# reference) and the port's OpenGL backend in one executable. Inputs are explicit; nothing is fetched.
cmake_minimum_required(VERSION 3.24)
project(rhi_conformance_host LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(VCS_UPSTREAM_DIR "" CACHE PATH "Overlaid upstream checkout")
set(VULKAN_HEADERS_DIR "" CACHE PATH "Vulkan-Headers include folder")
set(VOLK_DIR "" CACHE PATH "volk sources")
set(VMA_DIR "" CACHE PATH "VulkanMemoryAllocator include folder")
set(SHADER_HEADERS_DIR "" CACHE PATH "spirv/ and glsl/ shader headers")
set(IMGUI_DIR "" CACHE PATH "Dear ImGui (upstream's pin): the null backend takes its draw data")
foreach(required VCS_UPSTREAM_DIR VULKAN_HEADERS_DIR VOLK_DIR VMA_DIR SHADER_HEADERS_DIR IMGUI_DIR)
    if(NOT IS_DIRECTORY "${${required}}")
        message(FATAL_ERROR "Provide ${required}")
    endif()
endforeach()

find_library(EGL_LIBRARY NAMES EGL REQUIRED)
add_executable(rhi_conformance
    "${PORT_ROOT}/src/rhi_tests/host_main.cpp" "${PORT_ROOT}/src/rhi_tests/rhi_conformance.cpp"
    "${PORT_ROOT}/src/rhi/gl/rhi_gl.cpp" "${PORT_ROOT}/third_party/glad/src/gl.c"
    "${VCS_UPSTREAM_DIR}/src/rhi/rhi.cpp" "${VCS_UPSTREAM_DIR}/src/rhi/null/rhi_null.cpp"
    "${VCS_UPSTREAM_DIR}/src/rhi/vulkan/rhi_vulkan.cpp" "${VOLK_DIR}/volk.c"
    "${VCS_UPSTREAM_DIR}/src/render/gs_shaders.cpp"
    "${IMGUI_DIR}/imgui.cpp" "${IMGUI_DIR}/imgui_draw.cpp" "${IMGUI_DIR}/imgui_tables.cpp" "${IMGUI_DIR}/imgui_widgets.cpp")
target_include_directories(rhi_conformance PRIVATE "${PORT_ROOT}/src" "${VCS_UPSTREAM_DIR}/src" "${PORT_ROOT}/third_party/glad/include"
    "${VULKAN_HEADERS_DIR}" "${VOLK_DIR}" "${VMA_DIR}" "${SHADER_HEADERS_DIR}" "${IMGUI_DIR}")
target_compile_definitions(rhi_conformance PRIVATE VCS_HAVE_VULKAN=1 VCS_HAVE_OPENGL=1 VK_NO_PROTOTYPES)
target_compile_options(rhi_conformance PRIVATE -Wall -Wextra -ffp-contract=off)
target_link_libraries(rhi_conformance PRIVATE "${EGL_LIBRARY}" ${CMAKE_DL_LIBS} pthread)
