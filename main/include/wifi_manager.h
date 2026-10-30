/**
 * @file wifi_manager.h
 * @brief Gerenciamento e conexão Wi-Fi (Multi-SSID via NVS ou padrão Kconfig).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Carrega as credenciais Wi-Fi da NVS (lista Multi-SSID ou SSID único)
 *        ou mantém os valores padrão definidos no Kconfig.
 */
void wifi_manager_load_credentials(void);

/**
 * @brief Inicializa a pilha TCP/IP, o loop de eventos e o rádio Wi-Fi em modo Station
 *        (com Power Save desativado para baixa latência no WireGuard).
 */
void wifi_manager_init(void);

/**
 * @brief Bloqueia a execução até que a conexão Wi-Fi obtenha endereço IP (WIFI_CONNECTED_BIT).
 */
void wifi_manager_wait_connected(void);

#ifdef __cplusplus
}
#endif

