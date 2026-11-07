cmake_minimum_required(VERSION 3.20)
project(slate_shell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SLATE_BACKEND "sdl" CACHE STRING "Display backend: sdl or drm")
set_property(CACHE SLATE_BACKEND PROPERTY STRINGS sdl drm)

include(FetchContent)
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE FILEPATH "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.2.2.tar.gz
    URL_HASH SHA256=129b4e00e06639fa79d7e8a6cab3c1ecce2445b1a246652ccd34f22e7b17ad6f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lvgl)

find_package(PkgConfig REQUIRED)
if(SLATE_BACKEND STREQUAL "sdl")
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    target_compile_definitions(lvgl PUBLIC LV_USE_SDL=1)
    target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)
    set(SLATE_BACKEND_SOURCES ui/platform/sdl.c)
elseif(SLATE_BACKEND STREQUAL "drm")
    pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm)
    set(SLATE_BACKEND_SOURCES ui/platform/drm.c ui/platform/touch.c)
    set(SLATE_BACKEND_LIBS PkgConfig::DRM)
else()
    message(FATAL_ERROR "SLATE_BACKEND must be sdl or drm")
endif()

add_library(slateui STATIC
    ui/platform/platform.c
    ui/platform/headless.c
    ${SLATE_BACKEND_SOURCES}
    ui/theme/theme.c
    ui/theme/fonts/slate_font_14.c
    ui/theme/fonts/slate_font_16.c
    ui/theme/fonts/slate_font_20.c
    ui/theme/fonts/slate_font_24.c
    ui/theme/fonts/slate_font_32.c
    ui/theme/fonts/slate_font_48.c
    ui/nav/nav.c
    ui/components/status_bar.c
    ui/components/app_icon.c
    ui/components/header.c
    ui/components/icon.c
    ui/components/list.c
    ui/components/stat_card.c
    ui/components/wallpaper.c)
target_include_directories(slateui PUBLIC ui)
target_link_libraries(slateui PUBLIC lvgl m ${SLATE_BACKEND_LIBS})
target_compile_options(slateui PRIVATE -Wall -Wextra)

add_library(slateservices STATIC
    services/sysinfo.c)
target_include_directories(slateservices PUBLIC services)
target_compile_options(slateservices PRIVATE -Wall -Wextra)

add_library(slatescreens STATIC
    screens/desktop.c
    screens/app_stub.c
    screens/settings.c
    screens/about.c
    screens/diagnostics.c)
target_include_directories(slatescreens PUBLIC screens)
target_link_libraries(slatescreens PUBLIC slateui slateservices)
target_compile_options(slatescreens PRIVATE -Wall -Wextra)

add_executable(slate-shell main.c)
target_link_libraries(slate-shell PRIVATE slatescreens)
target_compile_options(slate-shell PRIVATE -Wall -Wextra)

add_executable(slate-snapshot tools/snapshot.c)
target_link_libraries(slate-snapshot PRIVATE slatescreens)

enable_testing()
add_subdirectory(tests)
