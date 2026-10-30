/**
 * @file tailscale_app.c
 * @brief Lógica de conexão e troca de mensagens com MicroLink v2 e Tailscale
 *        (baseado em basic_connect_tailscale).
 */

#include "tailscale_app.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "microlink.h"
#include "microlink_internal.h" /* Necessário para acesso aos handles de task para diagnósticos */

static const char *TAG = "main";

/* Porta UDP utilizada para teste de troca de mensagens sobre a VPN */
#define MSG_PORT 9000

/* Intervalo em milissegundos para envio periódico de mensagens de teste */
#define MSG_SEND_INTERVAL_MS 5000

/* Handle principal da instância do MicroLink (cliente Tailscale / WireGuard) */
static microlink_t *ml = NULL;

/* Handle do socket UDP criado sobre a pilha da VPN */
static microlink_udp_socket_t *udp_sock = NULL;

/* Contadores estatísticos de mensagens trafegadas pelo socket UDP */
static uint32_t msg_tx_count = 0;
static uint32_t msg_rx_count = 0;

/* IP de peer de destino configurado no Kconfig para teste de envio */
static uint32_t target_ip = 0;
static const char *target_ip_str = CONFIG_ML_EXAMPLE_TARGET_PEER_IP;

/* ============================================================================
 * Callback de Recepção UDP (executado a partir da task dedicada de RX do MicroLink)
 * ========================================================================== */

/**
 * @brief Função invocada automaticamente sempre que um pacote UDP chega na porta 9000 pela VPN.
 *
 * @param sock     Ponteiro para o socket UDP do MicroLink.
 * @param src_ip   Endereço IP do remetente (formato binário/uint32).
 * @param src_port Porta UDP de origem do remetente.
 * @param data     Buffer com os bytes brutos recebidos.
 * @param len      Quantidade de bytes recebidos.
 * @param user_data Contexto de usuário opcional (não utilizado aqui).
 */
static void on_udp_rx(microlink_udp_socket_t *sock, uint32_t src_ip, uint16_t src_port,
                      const uint8_t *data, size_t len, void *user_data)
{
    msg_rx_count++;
    char ip_str[16];
    microlink_ip_to_str(src_ip, ip_str);

    /* Copia os dados para uma string segura com terminador nulo ('\0') para log */
    char msg[256];
    size_t copy_len = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
    memcpy(msg, data, copy_len);
    msg[copy_len] = '\0';

    /* Remove quebra de linha ('\n') final caso o comando de teste a envie */
    if (copy_len > 0 && msg[copy_len - 1] == '\n')
        msg[copy_len - 1] = '\0';

    ESP_LOGI(TAG, "UDP RX #%lu de %s:%u [%d bytes]: \"%s\"",
             (unsigned long)msg_rx_count, ip_str, src_port, (int)len, msg);

    /* Envia uma mensagem de resposta de eco ("ECHO: ...") de volta para a origem */
    char reply[300];
    int reply_len = snprintf(reply, sizeof(reply), "ECHO: %s", msg);
    if (reply_len > 0)
    {
        esp_err_t err = microlink_udp_send(sock, src_ip, src_port, reply, reply_len);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "UDP TX echo -> %s:%u", ip_str, src_port);
        }
        else
        {
            ESP_LOGW(TAG, "Falha no envio do UDP echo: %d (handshake WireGuard em andamento)", err);
        }
    }
}

/* ============================================================================
 * Callbacks de Eventos do MicroLink
 * ========================================================================== */

/**
 * @brief Notificação de mudança na máquina de estados da conexão MicroLink/Tailscale.
 */
static void on_state_change(microlink_t *ml_handle, microlink_state_t state, void *user_data)
{
    const char *state_names[] = {
        "IDLE", "WIFI_WAIT", "CONNECTING", "REGISTERING",
        "CONNECTED", "RECONNECTING", "ERROR"};
    const char *name = (state < sizeof(state_names) / sizeof(state_names[0]))
                           ? state_names[state]
                           : "UNKNOWN";
    ESP_LOGI(TAG, "Estado MicroLink: %s", name);

    /* Quando a conexão com a rede Tailscale é estabelecida com sucesso */
    if (state == ML_STATE_CONNECTED)
    {
        uint32_t ip = microlink_get_vpn_ip(ml_handle);
        char ip_str[16];
        microlink_ip_to_str(ip, ip_str);
        ESP_LOGI(TAG, "Conectado ao Tailscale! VPN IP: %s", ip_str);
        ESP_LOGI(TAG, "=======================================================");
        ESP_LOGI(TAG, " Portal MicroLink (Tailscale): http://%s/", ip_str);
        ESP_LOGI(TAG, " Dashboard Web    (Tailscale): http://%s:%d/", ip_str, CONFIG_DASHBOARD_PORT);
        ESP_LOGI(TAG, "=======================================================");
    }
}

/**
 * @brief Invocado quando um peer da rede Tailscale altera status ou rota (direta ou relay DERP).
 */
static void on_peer_update(microlink_t *ml_handle, const microlink_peer_info_t *peer,
                           void *user_data)
{
    char ip_str[16];
    microlink_ip_to_str(peer->vpn_ip, ip_str);
    ESP_LOGI(TAG, "Peer: %s (%s) online=%d direct_path=%d",
             peer->hostname, ip_str, peer->online, peer->direct_path);
}

esp_err_t tailscale_app_start(void)
{
    /* Inicializa a pilha do MicroLink (cliente Tailscale / WireGuard).
     * A chave de autenticação (Auth Key) e o nome do dispositivo vêm inicialmente do Kconfig,
     * porém o microlink_init() sobrescreve automaticamente com valores salvos na NVS se existirem. */
    microlink_config_t config = {
        .auth_key = CONFIG_ML_TAILSCALE_AUTH_KEY,
        .device_name = CONFIG_ML_DEVICE_NAME,
        .enable_derp = true,  /* Habilita servidores relay DERP da Tailscale caso NAT traversal direto falhe */
        .enable_stun = true,  /* Habilita STUN para descoberta de IP/porta pública e conexão ponto a ponto */
        .enable_disco = true, /* Habilita protocolo Discovery (DISCO) da Tailscale */
        .max_peers = CONFIG_ML_MAX_PEERS,
        .wifi_tx_power_dbm = 13, /* Potência de transmissão Wi-Fi reduzida para controle térmico do ESP32 */
    };

    ml = microlink_init(&config);
    if (!ml)
    {
        ESP_LOGE(TAG, "Falha ao inicializar o MicroLink");
        return ESP_FAIL;
    }

    /* Registra os callbacks de monitoramento de status da conexão e atualização de peers */
    microlink_set_state_callback(ml, on_state_change, NULL);
    microlink_set_peer_callback(ml, on_peer_update, NULL);

    /* Dispara as tarefas de background do MicroLink para autenticação e conexão com a control plane */
    ESP_ERROR_CHECK(microlink_start(ml));

    /* Aguarda até que o dispositivo esteja autenticado e com túnel WireGuard ativo na rede Tailscale */
    while (!microlink_is_connected(ml))
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Cria o socket UDP na porta 9000 sobre a interface da VPN para comunicação direta entre nós */
    udp_sock = microlink_udp_create(ml, MSG_PORT);
    if (!udp_sock)
    {
        ESP_LOGE(TAG, "Falha ao criar socket UDP do MicroLink");
    }
    else
    {
        ESP_LOGI(TAG, "Socket UDP ouvindo na porta %d da VPN", MSG_PORT);
        microlink_udp_set_rx_callback(udp_sock, on_udp_rx, NULL);
    }

    /* Avalia se há um IP de peer de destino configurado no Kconfig para teste de envio */
    if (target_ip_str && target_ip_str[0] != '\0')
    {
        target_ip = microlink_parse_ip(target_ip_str);
        if (target_ip != 0)
        {
            ESP_LOGI(TAG, "Configurado para enviar mensagens a %s:%d a cada %dms",
                     target_ip_str, MSG_PORT, MSG_SEND_INTERVAL_MS);
        }
        else
        {
            ESP_LOGW(TAG, "Endereço IP de destino inválido: '%s'", target_ip_str);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Nenhum IP de peer de destino configurado (operando em modo receive-only)");
    }

    return ESP_OK;
}

void tailscale_app_loop(void)
{
    uint64_t last_send_ms = 0;
    uint64_t last_status_ms = 0;

    while (1)
    {
        /* Intervalo de ciclo de 1 segundo para tarefas periódicas */
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint64_t now = (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* Envio periódico de mensagem UDP para o peer de destino (caso configurado) */
        if (udp_sock && target_ip != 0 && now - last_send_ms >= MSG_SEND_INTERVAL_MS)
        {
            last_send_ms = now;
            msg_tx_count++;
            char msg[128];
            int msg_len = snprintf(msg, sizeof(msg), "hello from ESP32 #%lu", (unsigned long)msg_tx_count);
            esp_err_t err = microlink_udp_send(udp_sock, target_ip, MSG_PORT, msg, msg_len);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "UDP TX #%lu -> %s:%d: \"%s\"",
                         (unsigned long)msg_tx_count, target_ip_str, MSG_PORT, msg);
            }
            else
            {
                ESP_LOGW(TAG, "UDP TX #%lu FALHOU (err=%d, handshake WireGuard em andamento)",
                         (unsigned long)msg_tx_count, err);
            }
        }

        /* Relatório periódico de status e saúde do sistema a cada 30 segundos */
        if (now - last_status_ms >= 30000)
        {
            last_status_ms = now;

            if (microlink_is_connected(ml))
            {
                int peer_count = microlink_get_peer_count(ml);
                ESP_LOGI(TAG, "Status: CONECTADO | Peers: %d | TX: %lu | RX: %lu | Heap: %lu",
                         peer_count, (unsigned long)msg_tx_count, (unsigned long)msg_rx_count,
                         (unsigned long)esp_get_free_heap_size());

                /* Lista os peers conhecidos e se o tráfego com eles é DIRECT (P2P) ou DERP (Relay) */
                for (int i = 0; i < peer_count; i++)
                {
                    microlink_peer_info_t info;
                    if (microlink_get_peer_info(ml, i, &info) == ESP_OK)
                    {
                        char ip_str[16];
                        microlink_ip_to_str(info.vpn_ip, ip_str);
                        ESP_LOGI(TAG, "  [%d] %s (%s) %s",
                                  i, info.hostname, ip_str,
                                  info.direct_path ? "DIRECT" : "DERP");
                    }
                }
            }

            /* Diagnóstico de uso de memória de pilha (stack high-water mark) das tarefas do MicroLink */
            {
                static const char *sn[] = {
                    "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"};
                struct
                {
                    const char *name;
                    TaskHandle_t handle;
                } tasks[] = {
                    {"net_io", ml->net_io_task},
                    {"derp_tx", ml->derp_tx_task},
                    {"coord", ml->coord_task},
                    {"wg_mgr", ml->wg_mgr_task},
                };
                for (int t = 0; t < 4; t++)
                {
                    if (tasks[t].handle)
                    {
                        eTaskState st = eTaskGetState(tasks[t].handle);
                        int si = (st <= eInvalid) ? (int)st : 5;
                        ESP_LOGW(TAG, "TASK[%s]: estado=%s stack_free=%lu",
                                 tasks[t].name, sn[si],
                                 (unsigned long)uxTaskGetStackHighWaterMark(tasks[t].handle));
                    }
                }
            }
        }
    }
}

