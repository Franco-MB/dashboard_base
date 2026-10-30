/**
 * @file wifi_manager.c
 * @brief Gerenciamento e conexão Wi-Fi (baseado em basic_connect_tailscale).
 */

#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "ml_config_httpd.h" /* Permite ler/sobrescrever credenciais Wi-Fi salvas em NVS */

static const char *TAG = "main";

/* Credenciais Wi-Fi: iniciam com os valores definidos no Kconfig; a NVS pode sobrescrevê-las */
static char wifi_ssid[33] = CONFIG_ML_WIFI_SSID;
static char wifi_password[65] = CONFIG_ML_WIFI_PASSWORD;

/* Gerenciamento de múltiplos pontos de acesso (Multi-SSID) salvos na NVS */
static ml_config_wifi_list_t wifi_list;
static int wifi_list_count = 0; /* 0 indica modo de SSID único */
static int current_wifi_idx = 0;
static int wifi_retry_count = 0;
#define WIFI_MAX_RETRIES_PER_SSID 3

/* Grupo de eventos do FreeRTOS para sincronização do estado de conexão Wi-Fi */
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* ============================================================================
 * Gerenciamento e Conexão Wi-Fi
 * ========================================================================== */

/**
 * @brief Alterna para a próxima rede Wi-Fi cadastrada na lista (modo round-robin).
 * Caso exista apenas uma rede, apenas solicita reconexão.
 */
static void wifi_try_next(void)
{
    if (wifi_list_count <= 1)
    {
        /* Apenas um SSID: tenta reconectar diretamente */
        esp_wifi_connect();
        return;
    }

    wifi_retry_count++;
    if (wifi_retry_count >= WIFI_MAX_RETRIES_PER_SSID)
    {
        /* Atingiu o limite de tentativas para a rede atual, avança para a próxima */
        wifi_retry_count = 0;
        current_wifi_idx = (current_wifi_idx + 1) % wifi_list_count;
    }

    /* Atualiza a configuração da interface Station com as credenciais da próxima rede */
    ml_config_wifi_entry_t *e = &wifi_list.entries[current_wifi_idx];
    wifi_config_t wifi_config = {
        .sta = {.threshold.authmode = WIFI_AUTH_WPA2_PSK},
    };
    strncpy((char *)wifi_config.sta.ssid, e->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, e->pass, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Tentando Wi-Fi #%d/%d: %s (tentativa %d/%d)",
             current_wifi_idx + 1, wifi_list_count, e->ssid,
             wifi_retry_count + 1, WIFI_MAX_RETRIES_PER_SSID);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();
}

/**
 * @brief Manipulador de eventos de sistema para Wi-Fi e obtenção de IP.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        /* Driver Wi-Fi iniciado, dispara a tentativa de conexão */
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Wi-Fi desconectado, reason=%d", disc->reason);
        wifi_try_next();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        /* Placa conectada ao ponto de acesso e com IP local atribuído */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi conectado a %s, IP local: " IPSTR,
                 wifi_list_count > 0 ? wifi_list.entries[current_wifi_idx].ssid : wifi_ssid,
                 IP2STR(&event->ip_info.ip));
        wifi_retry_count = 0;
        /* Libera a task principal que aguardava por conectividade */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_load_credentials(void)
{
    /* Verifica se existem credenciais Wi-Fi salvas na NVS (inseridas via interface web de configuração) */
    memset(&wifi_list, 0, sizeof(wifi_list));
    wifi_list.active_idx = 0xFF;

    if (ml_config_get_wifi_list(&wifi_list) && wifi_list.count > 1)
    {
        /* Modo multi-SSID habilitado */
        wifi_list_count = wifi_list.count;
        current_wifi_idx = 0;
        strncpy(wifi_ssid, wifi_list.entries[0].ssid, sizeof(wifi_ssid) - 1);
        strncpy(wifi_password, wifi_list.entries[0].pass, sizeof(wifi_password) - 1);
        ESP_LOGI(TAG, "Wi-Fi multi-SSID detectado: %d redes (iniciando por: %s)", wifi_list_count, wifi_ssid);
        for (int i = 0; i < wifi_list_count; i++)
        {
            ESP_LOGI(TAG, "  Rede Wi-Fi #%d: %s", i + 1, wifi_list.entries[i].ssid);
        }
    }
    else if (ml_config_get_nvs_wifi(wifi_ssid, sizeof(wifi_ssid),
                                    wifi_password, sizeof(wifi_password)))
    {
        ESP_LOGI(TAG, "Utilizando credenciais Wi-Fi recuperadas da NVS: %s", wifi_ssid);
    }
    else
    {
        ESP_LOGI(TAG, "Utilizando credenciais Wi-Fi padrão do Kconfig: %s", wifi_ssid);
    }
}

void wifi_manager_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Registro dos handlers para eventos de Wi-Fi e IP */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Desativa o modo de economia de energia (Power Save) do Wi-Fi para tráfego WireGuard de baixa latência.
     * O padrão do ESP-IDF (WIFI_PS_MIN_MODEM) adiciona latência de até um intervalo DTIM (100 a 300ms)
     * por ciclo de despertar para recebimento de pacotes. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "Inicialização do Wi-Fi concluída (PS=NONE), conectando a %s...", wifi_ssid);
}

void wifi_manager_wait_connected(void)
{
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
}

