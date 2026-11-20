cmake_minimum_required(VERSION 3.16)
project(clock_preview C)
set(CMAKE_C_STANDARD 11)
get_filename_component(CLOCK_ROOT "../.." ABSOLUTE)
set(LV_CONF_PATH "${CLOCK_ROOT}/include/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory("${CLOCK_ROOT}/managed_components/lvgl__lvgl" lvgl)
file(GLOB CLOCK_FONTS "${CLOCK_ROOT}/src/fonts/*.c")
add_executable(clock_preview
    main.c
    "${CLOCK_ROOT}/src/clock_ui.c"
    "${CLOCK_ROOT}/src/flip_digit.c"
    ${CLOCK_FONTS}
)
target_include_directories(clock_preview PRIVATE "${CLOCK_ROOT}/src" "${CLOCK_ROOT}/src/fonts")
target_compile_options(clock_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(clock_preview PRIVATE lvgl m)
