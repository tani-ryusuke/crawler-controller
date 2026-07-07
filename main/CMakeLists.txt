idf_component_register(SRCS "main.c" "esp_now_handler.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES led_strip)