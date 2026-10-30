/**
 * @file dashboard_portal.h
 * @brief Servidor HTTP e WebSocket (/ws) do Dashboard na porta CONFIG_DASHBOARD_PORT (padrão 8080).
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicia o servidor HTTP + WebSocket do Dashboard na porta CONFIG_DASHBOARD_PORT (padrão 8080)
 *        e cria a tarefa periódica de envio de mensagens ("ola N") via WebSocket.
 *
 * @return ESP_OK em caso de sucesso, ou código de erro do esp_http_server.
 */
esp_err_t dashboard_portal_start(void);

#ifdef __cplusplus
}
#endif

