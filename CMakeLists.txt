cmake_minimum_required(VERSION 3.24)

# Supply the official toolchain on the FIRST configure; never set compilers here.
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake")
endif()
project(gtavcs_nx VERSION 0.1.0 LANGUAGES C CXX)

if(NOT NINTENDO_SWITCH OR NOT COMMAND nx_create_nro)
    message(FATAL_ERROR "The official devkitPro Switch toolchain and switch-cmake package are required")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEVKITPRO)
    set(DEVKITPRO "$ENV{DEVKITPRO}")
endif()
set(SWITCH_PORTLIBS "${DEVKITPRO}/portlibs/switch")
# Never ask Ubuntu's pkg-config for host SDL, GL, X11 or Wayland dependencies.
find_program(PKG_CONFIG_EXECUTABLE NAMES aarch64-none-elf-pkg-config
    PATHS "${SWITCH_PORTLIBS}/bin" NO_DEFAULT_PATH REQUIRED)
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "${SWITCH_PORTLIBS}/lib/pkgconfig:${SWITCH_PORTLIBS}/share/pkgconfig")
find_package(PkgConfig REQUIRED)
pkg_check_modules(NX_SDL2 REQUIRED sdl2>=2.0.7)
# Switch Mesa exposes desktop entry points through EGL + glapi; no gl.pc.
pkg_check_modules(NX_GL REQUIRED glapi)
pkg_check_modules(NX_EGL REQUIRED egl)
find_library(NX_LIBRARY NAMES nx PATHS "${DEVKITPRO}/libnx/lib" NO_DEFAULT_PATH REQUIRED)

add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# This milestone deliberately builds only the platform smoke test. The upstream
# root CMakeLists is x86/Clang/SDL3 specific and cannot yet be add_subdirectory'd.
add_executable(gtavcs src/main.c src/core_stub.c)
set_target_properties(gtavcs PROPERTIES SUFFIX ".elf" LINKER_LANGUAGE CXX)
target_include_directories(gtavcs PRIVATE src
    ${NX_SDL2_INCLUDE_DIRS} ${NX_GL_INCLUDE_DIRS} ${NX_EGL_INCLUDE_DIRS})
target_compile_definitions(gtavcs PRIVATE SDL_MAIN_HANDLED)
execute_process(COMMAND dkp-pacman -Q libnx OUTPUT_VARIABLE NX_PACKAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
target_compile_definitions(gtavcs PRIVATE GTAVCS_LIBNX_PACKAGE="${NX_PACKAGE}")
target_compile_options(gtavcs PRIVATE -Wall -Wextra -Wpedantic
    -ffp-contract=off -fno-strict-aliasing
    ${NX_SDL2_CFLAGS_OTHER} ${NX_GL_CFLAGS_OTHER} ${NX_EGL_CFLAGS_OTHER})
target_link_directories(gtavcs PRIVATE "${SWITCH_PORTLIBS}/lib")
target_link_options(gtavcs PRIVATE
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/gtavcs.map" -Wl,--gc-sections
    ${NX_SDL2_STATIC_LDFLAGS_OTHER} ${NX_GL_STATIC_LDFLAGS_OTHER} ${NX_EGL_STATIC_LDFLAGS_OTHER})
# Portlibs are static. Grouping resolves Mesa/SDL dependencies without depending
# on incidental archive ordering. switch.specs/PIE/TLS flags come from Switch.cmake.
target_link_libraries(gtavcs PRIVATE glad -Wl,--start-group
    ${NX_SDL2_STATIC_LIBRARIES} ${NX_GL_STATIC_LIBRARIES} ${NX_EGL_STATIC_LIBRARIES}
    "${NX_LIBRARY}" m -Wl,--end-group)

nx_generate_nacp(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs.nacp"
    NAME "GTA VCS NX - platform smoke" AUTHOR "gtavcs_nx contributors" VERSION "${PROJECT_VERSION}")
nx_create_nro(gtavcs OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gtavcs.nro"
    NACP "${CMAKE_CURRENT_BINARY_DIR}/gtavcs.nacp")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gtavcs.nro" DESTINATION switch/gtavcs_nx)
