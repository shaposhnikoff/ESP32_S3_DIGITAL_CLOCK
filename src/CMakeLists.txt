file(GLOB APP_SOURCES "*.c" "fonts/*.c")
idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS "." "fonts" "../include" "../.pio/generated"
    REQUIRES esp_lcd esp_driver_gpio esp_driver_i2c esp_timer esp_wifi esp_event
             esp_netif lwip nvs_flash lvgl esp_http_client mbedtls json
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
