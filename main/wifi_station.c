/*
 * ============================================================================
 * wifi_station.c — Conexao Wi-Fi Station (padrão oficial da API ESP-IDF)
 * Baseado em: esp-idf/examples/wifi/getting_started/station
 *
 * Como configurar sua rede Wi-Fi:
 *   1. Rode: idf.py menuconfig
 *   2. Entre em: "Configuracao do Wi-Fi do Dashboard"
 *   3. Altere "WiFi SSID" e "WiFi Password"
 *   (Ou edite os valores default em main/Kconfig.projbuild)
 * ============================================================================
 */

#include "wifi_station.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

/* Valores vindos do menuconfig (main/Kconfig.projbuild) */
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* Event group do FreeRTOS para sincronizar a espera pelo IP */
static EventGroupHandle_t s_wifi_event_group;

/* Bits que indicam se conectou com sucesso ou se falhou apos N tentativas */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WIFI_STA";
static int s_retry_num = 0;

/* ============================================================================
 * Callback chamado automaticamente pelo ESP-IDF quando ocorrem eventos de
 * Wi-Fi (iniciou, desconectou) ou de IP (recebeu IP do roteador).
 * ============================================================================ */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* 1. Driver Wi-Fi iniciou -> pede para conectar ao roteador */
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* 2. Caiu ou falhou -> tenta novamente ate o limite configurado */
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Tentando reconectar ao Wi-Fi... (%d/%d)",
                     s_retry_num, EXAMPLE_ESP_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* 3. Conectou e ganhou IP! Mostra o endereco no monitor serial */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "=====================================================");
        ESP_LOGI(TAG, " Conectado! Abra no navegador: http://" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "=====================================================");
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    /* Cria a interface de rede padrao para Wi-Fi Station */
    esp_netif_create_default_wifi_sta();

    /* Inicializa o driver Wi-Fi com a configuracao padrao */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Registra os callbacks para eventos WIFI_EVENT e IP_EVENT */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* Preenche SSID e senha definidos no menuconfig */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Caso sua rede seja aberta (sem senha), troque para WIFI_AUTH_OPEN */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Iniciando Wi-Fi Station (SSID: %s)...", EXAMPLE_ESP_WIFI_SSID);

    /* Bloqueia aqui ate conectar (WIFI_CONNECTED_BIT) ou falhar (WIFI_FAIL_BIT) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    }

    ESP_LOGE(TAG, "Falha ao conectar na rede SSID: %s", EXAMPLE_ESP_WIFI_SSID);
    return false;
}

