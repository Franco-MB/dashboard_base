/**
 * @file main.c
 * @brief Aplicação principal: Dashboard Base + MicroLink v2 (Tailscale)
 *
 * Visão Geral do Funcionamento:
 * 1. Inicializa o subsistema de armazenamento não-volátil (NVS Flash).
 * 2. Recupera credenciais Wi-Fi (da NVS via portal web ou padrões do Kconfig).
 * 3. Conecta-se à rede Wi-Fi e aguarda a obtenção de endereço IP local (DHCP).
 * 4. Inicia o servidor HTTP + WebSocket do Dashboard na porta 8080.
 * 5. Inicializa o cliente MicroLink v2 (com portal de configuração na porta 80)
 *    e registra o dispositivo na rede VPN Tailscale.
 * 6. Abre um socket UDP na porta 9000 pela interface VPN para receber mensagens (echo)
 *    e opcionalmente enviar mensagens periódicas a um peer configurado.
 *
 * Como testar a partir de outro dispositivo na mesma rede Tailscale:
 *   Portal MicroLink: http://<ESP32_VPN_IP>/
 *   Dashboard Web:    http://<ESP32_VPN_IP>:8080/
 *   Receber dados:    nc -lu 9000
 *   Enviar dados:     echo "olá do PC" | nc -u <ESP32_VPN_IP> 9000
 */

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "wifi_manager.h"
#include "dashboard_portal.h"
#include "tailscale_app.h"

static const char *TAG = "main";

/* ============================================================================
 * Ponto de Entrada da Aplicação (app_main)
 * ========================================================================== */

void app_main(void)
{
    /* 1. Inicializa a memória flash NVS (necessária para parâmetros Wi-Fi e chaves criptográficas do MicroLink) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /* Caso a partição NVS esteja corrompida ou com versão incompatível, apaga e recria */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "MicroLink v2 + Dashboard WebSocket");
    ESP_LOGI(TAG, "Heap livre: %lu bytes (PSRAM livre: %lu bytes)",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* 2. Verifica se existem credenciais Wi-Fi salvas na NVS (inseridas via interface web de configuração) */
    wifi_manager_load_credentials();

    /* 3. Inicia o rádio Wi-Fi e dispara o processo de conexão */
    wifi_manager_init();

    /* 4. Bloqueia a execução até que o evento de IP recebido (WIFI_CONNECTED_BIT) seja disparado */
    wifi_manager_wait_connected();

    /* 5. Inicia o Dashboard Web + WebSocket (porta HTTP configurada, padrão 8080) */
    if (dashboard_portal_start() == ESP_OK)
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            ESP_LOGI(TAG, "=======================================================");
            ESP_LOGI(TAG, " Portal MicroLink acessível em: http://" IPSTR "/",
                     IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, " Dashboard Web acessível em:    http://" IPSTR ":%d/",
                     IP2STR(&ip_info.ip), CONFIG_DASHBOARD_PORT);
            ESP_LOGI(TAG, "=======================================================");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao iniciar o Dashboard Web");
    }

    /* 6. Inicializa a pilha do MicroLink (cliente Tailscale / WireGuard) e o socket UDP 9000 */
    if (tailscale_app_start() != ESP_OK)
    {
        return;
    }

    /* 7. Loop principal de aplicação (envio periódico UDP e relatório de status/diagnóstico) */
    tailscale_app_loop();
}

