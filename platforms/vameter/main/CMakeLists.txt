
file(GLOB_RECURSE APP_LAYER_SRCS
    ../../../app/*.c
    ../../../app/*.cpp
)

list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*/app/libs/measurement_pipeline/src/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*/app/libs/signed_current_observation/.*")

if(CONFIG_VAMETER_SIGNED_CURRENT_OBSERVATION)
    list(APPEND APP_LAYER_SRCS
        ../../../app/libs/signed_current_observation/signed_current_observation.cpp
    )
endif()

set(APP_LAYER_INCS
    ../../../app
)

file(GLOB_RECURSE HAL_VAMETER_SRCS
    ./hal_vameter/*.c
    ./hal_vameter/*.cpp
)

list(FILTER HAL_VAMETER_SRCS EXCLUDE REGEX ".*/signed_current_observation_device.cpp")

if(CONFIG_VAMETER_SIGNED_CURRENT_OBSERVATION)
    list(APPEND HAL_VAMETER_SRCS
        ./hal_vameter/components/signed_current_observation_device.cpp
    )
endif()

idf_component_register(
    SRCS
        "vameter_user_demo.cpp"
        ${APP_LAYER_SRCS}
        ${HAL_VAMETER_SRCS}
    INCLUDE_DIRS
        "."
        ${APP_LAYER_INCS}
    REQUIRES
        measurement_pipeline
        hal
        mooncake
        LovyanGFX
        smooth_ui_toolkit
        ArduinoJson
    PRIV_REQUIRES
        ESP32Encoder
        arduino-esp32
        app_update
        esp_https_ota
        esp_http_client
        esp_wifi
        esp_netif
        mdns
        PsychicHttp
        esp_http_server
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LOG_LOCAL_LEVEL=ESP_LOG_INFO)
