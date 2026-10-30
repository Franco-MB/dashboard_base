# main/CMakeLists.txt — Registra os módulos em src/ e include/ e embute os 3 arquivos web na Flash
idf_component_register(
    SRCS
        "src/main.c"
        "src/wifi_manager.c"
        "src/tailscale_app.c"
        "src/dashboard_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES
        microlink
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        esp_timer
        json
    EMBED_FILES
        "web_gui/index.html"
        "web_gui/style.css"
        "web_gui/app.js"
)
