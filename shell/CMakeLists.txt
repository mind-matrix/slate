cmake_minimum_required(VERSION 3.20)
project(slate_shell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SLATE_BACKEND "sdl" CACHE STRING "Display backend: sdl (simulator) or wlr (the device, as a Wayland compositor)")
set_property(CACHE SLATE_BACKEND PROPERTY STRINGS sdl wlr)

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
    set(SLATE_BACKEND_SOURCES ui/platform/sdl.c ui/platform/windows_stub.c)
    set(SLATE_SYSTEM_SOURCES
        services/fake/net.c
        services/fake/bt.c
        services/fake/audio.c
        services/fake/backlight.c
        services/fake/power.c
        services/fake/update.c
        services/fake/timesync.c
        services/fake/chime.c
        services/fake/apps.c
        services/fake/voice.c)
elseif(SLATE_BACKEND STREQUAL "wlr")
    pkg_check_modules(WLR REQUIRED IMPORTED_TARGET wlroots-0.18 wayland-server xkbcommon pixman-1 libdrm)
    pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    set(PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/protocols)
    file(MAKE_DIRECTORY ${PROTOCOL_DIR})
    add_custom_command(OUTPUT ${PROTOCOL_DIR}/xdg-shell-protocol.h
        COMMAND ${WAYLAND_SCANNER} server-header ${WAYLAND_PROTOCOLS}/stable/xdg-shell/xdg-shell.xml ${PROTOCOL_DIR}/xdg-shell-protocol.h)
    add_custom_command(OUTPUT ${PROTOCOL_DIR}/wlr-layer-shell-unstable-v1-protocol.h
        COMMAND ${WAYLAND_SCANNER} server-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
            ${PROTOCOL_DIR}/wlr-layer-shell-unstable-v1-protocol.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml)
    set(SLATE_BACKEND_SOURCES
        compositor/server.c
        compositor/windows.c
        compositor/overlays.c
        compositor/input.c
        ui/platform/keyboard.c
        ${PROTOCOL_DIR}/xdg-shell-protocol.h
        ${PROTOCOL_DIR}/wlr-layer-shell-unstable-v1-protocol.h)
    set(SLATE_BACKEND_INCLUDES ${PROTOCOL_DIR})
    set(SLATE_BACKEND_DEFINES WLR_USE_UNSTABLE)
    set(SLATE_SYSTEM_SOURCES
        services/net_nm.c
        services/bt_bluez.c
        services/audio_wpctl.c
        services/backlight_sysfs.c
        services/power_logind.c
        services/update_systemd.c
        services/timesync.c
        services/chime_pwplay.c
        services/apps_system.c
        services/voice_system.c)
    set(SLATE_BACKEND_LIBS PkgConfig::WLR)
else()
    message(FATAL_ERROR "SLATE_BACKEND must be sdl or wlr")
endif()

add_library(slatecore STATIC
    compositor/access.c
    compositor/geometry.c
    system/keystore.c
    system/assistant.c)
target_include_directories(slatecore PUBLIC compositor system)
target_compile_options(slatecore PRIVATE -Wall -Wextra)

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
    ui/theme/fonts/slate_mono_18.c
    ui/theme/fonts/slate_digits_96.c
    ui/nav/nav.c
    ui/components/status_bar.c
    ui/components/app_icon.c
    ui/components/app_logo.c
    ui/components/button.c
    ui/components/dialog.c
    ui/components/header.c
    ui/components/icon.c
    ui/components/keyboard.c
    ui/components/list.c
    ui/components/quick_tile.c
    ui/components/roller.c
    ui/components/segmented.c
    ui/components/slider_row.c
    ui/components/stat_card.c
    ui/components/switch.c
    ui/components/wallpaper.c)
target_include_directories(slateui PUBLIC ui PRIVATE ${SLATE_BACKEND_INCLUDES})
target_compile_definitions(slateui PRIVATE ${SLATE_BACKEND_DEFINES})
target_link_libraries(slateui PUBLIC lvgl m slatecore ${SLATE_BACKEND_LIBS})
target_compile_options(slateui PRIVATE -Wall -Wextra)

add_library(slateservices STATIC
    services/sysinfo.c
    services/timezone.c
    services/bus.c
    services/net.c
    services/bt.c
    services/audio.c
    services/backlight.c
    services/prefs.c
    services/tty.c
    services/update.c
    services/notify.c
    services/clock.c
    services/chime.c
    services/apps.c
    services/voice.c
    ${SLATE_SYSTEM_SOURCES})
target_include_directories(slateservices PUBLIC services)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm)
target_link_libraries(slateservices PUBLIC slatecore PkgConfig::SYSTEMD)
target_compile_options(slateservices PRIVATE -Wall -Wextra)

add_library(slatescreens STATIC
    screens/desktop.c
    screens/settings.c
    screens/settings_ui.c
    screens/about.c
    screens/diagnostics.c
    screens/datetime.c
    screens/timezone_picker.c
    screens/quick_drawer.c
    screens/wifi.c
    screens/wifi_join.c
    screens/bluetooth.c
    screens/display_settings.c
    screens/idle.c
    screens/sound.c
    screens/power_screen.c
    screens/terminal.c
    screens/software_update.c
    screens/notification_hub.c
    screens/clock_app.c
    screens/clock_sheets.c
    screens/clock_alerts.c
    screens/apps_screen.c
    screens/app_detail.c
    screens/voice_assistant.c
    screens/open_apps.c)
target_include_directories(slatescreens PUBLIC screens)
target_link_libraries(slatescreens PUBLIC slateui slateservices PkgConfig::VTERM)
target_compile_options(slatescreens PRIVATE -Wall -Wextra)

add_executable(slate-shell main.c)
target_link_libraries(slate-shell PRIVATE slatescreens)
target_compile_options(slate-shell PRIVATE -Wall -Wextra)

add_executable(slate-snapshot tools/snapshot.c)
target_link_libraries(slate-snapshot PRIVATE slatescreens)

# Slate's shared mDNS daemon: Apple's mDNSResponder, pinned and checked, built without TLS.
# Apps bring their own DNS-SD client and reach it through DNSSD_UDS_PATH (see APPS.md).
if(SLATE_BACKEND STREQUAL "wlr")
    include(ExternalProject)
    ExternalProject_Add(mdnsresponder
        URL https://github.com/apple-oss-distributions/mDNSResponder/archive/refs/tags/mDNSResponder-2881.120.11.tar.gz
        URL_HASH SHA256=f665cf02b5c930f9b70fb39b7567f03dd821997594c56ce399098eaa0372fb15
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/patch-mdnsd.sh
        CONFIGURE_COMMAND ""
        BUILD_IN_SOURCE TRUE
        BUILD_COMMAND make -C mDNSPosix os=linux tls=no Daemon
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy mDNSPosix/build/prod/mdnsd ${CMAKE_BINARY_DIR}/mdnsd)
endif()

if(SLATE_BACKEND STREQUAL "wlr")
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_executable(slate-system system/main.c system/vault.c)
    target_link_libraries(slate-system PRIVATE slatecore PkgConfig::SYSTEMD PkgConfig::SODIUM)
    target_compile_options(slate-system PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_subdirectory(tests)
