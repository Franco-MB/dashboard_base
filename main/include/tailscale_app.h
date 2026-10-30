/**
 * @file tailscale_app.h
 * @brief Inicialização e loop principal do cliente MicroLink v2 (Tailscale / WireGuard).
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o MicroLink v2, aguarda conexão à rede Tailscale e abre o socket UDP (porta 9000).
 *
 * @return ESP_OK em caso de sucesso, ESP_FAIL se falhar ao inicializar o MicroLink.
 */
esp_err_t tailscale_app_start(void);

/**
 * @brief Executa o loop principal de envio periódico UDP e relatório de status/diagnóstico.
 *        Esta função não retorna.
 */
void tailscale_app_loop(void);

#ifdef __cplusplus
}
#endif

