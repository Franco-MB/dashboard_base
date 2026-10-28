# main/CMakeLists.txt — Registra o codigo C e embute os 3 arquivos web na Flash
idf_component_register(
    SRCS "main.c" "wifi_station.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server esp_wifi nvs_flash esp_netif esp_event json
    EMBED_FILES
        "web_gui/index.html"
        "web_gui/style.css"
        "web_gui/app.js"
)
