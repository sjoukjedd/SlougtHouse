idf_component_register(
    SRCS
        "main.c"
        "synth_signals.c"
        "ble_dummy.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        bt
        freertos
        log
        esp_timer
        nvs_flash
)
