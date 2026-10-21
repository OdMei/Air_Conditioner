cmake_minimum_required(VERSION 3.21)
project(n647_lvgl_preview LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(APP_CORE "${APP_ROOT}/Project/Project/Air_Conditioner/Appli/Core")
set(SDL2_ROOT "C:/msys64/mingw64" CACHE PATH "SDL2 development files for the selected compiler")

# The installed MSYS2 SDL2 package can be used without a CMake package file.
find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
    find_path(SDL2_INCLUDE_DIR NAMES SDL.h
        HINTS "${SDL2_ROOT}/include" PATH_SUFFIXES SDL2)
    find_library(SDL2_LIBRARY NAMES SDL2 HINTS "${SDL2_ROOT}/lib")
    if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
        message(FATAL_ERROR "SDL2 headers/import library were not found. Set SDL2_ROOT to the installed SDL2 prefix.")
    endif()
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
endif()

# This path is private to the desktop build; the MCU keeps its existing config.
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "Desktop LVGL configuration" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${APP_ROOT}/Middlewares/LVGL" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")

add_executable(lvgl_preview
    main.c
    pc_port.c
    "${APP_CORE}/Src/app_ui.c"
    "${APP_CORE}/Src/app_ui_watch_home.c"
    "${APP_CORE}/Src/app_ui_temperature_input.c"
    "${APP_CORE}/Src/app_ui_watch_assets.c"
    "${APP_CORE}/Src/app_ui_theme.c"
    "${APP_CORE}/Src/app_environment_alert.c"
    "${APP_CORE}/Src/app_event_journal.c"
    "${APP_CORE}/Src/app_ui_environment.c"
    "${APP_CORE}/Src/app_ui_journal.c"
    "${APP_CORE}/Src/app_ui_controls.c"
    "${APP_CORE}/Src/app_detection_smoother.c"
    "${APP_CORE}/Src/app_ui_insights.c"
    "${APP_CORE}/Src/app_history.c"
    "${APP_CORE}/Src/app_demo_engine.c"
    "${APP_CORE}/Src/app_ui_pixels.c"
    "${APP_CORE}/Src/app_perf_window.c"
    "${APP_CORE}/Src/app_font_ui_16.c"
    "${APP_CORE}/Src/app_climate_logic.c"
    "${APP_CORE}/Src/app_input_logic.c")
target_compile_definitions(lvgl_preview PRIVATE APP_SIMULATOR=1 SDL_MAIN_HANDLED)
target_include_directories(lvgl_preview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_CORE}/Inc")
target_link_libraries(lvgl_preview PRIVATE lvgl::lvgl SDL2::SDL2)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lvgl_preview PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(lvgl_preview PRIVATE m)
endif()
if(MINGW)
    target_link_options(lvgl_preview PRIVATE -static-libgcc)
endif()
set_target_properties(lvgl_preview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32)
    find_file(SDL2_RUNTIME_DLL NAMES SDL2.dll HINTS "${SDL2_ROOT}/bin")
    if(SDL2_RUNTIME_DLL)
        add_custom_command(TARGET lvgl_preview POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${SDL2_RUNTIME_DLL}" "$<TARGET_FILE_DIR:lvgl_preview>/SDL2.dll")
    endif()
endif()
