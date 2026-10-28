/*
 * ============================================================================
 * wifi_station.h — Conexao Wi-Fi em modo Station (baseado no exemplo oficial
 * examples/wifi/getting_started/station do ESP-IDF)
 * ============================================================================
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Inicializa o Wi-Fi em modo Station (STA) e aguarda conectar e obter IP.
 *
 * As credenciais (SSID, senha e tentativas) sao lidas do menuconfig:
 *   idf.py menuconfig -> "Configuracao do Wi-Fi do Dashboard"
 *   (definidas no arquivo main/Kconfig.projbuild)
 *
 * @return true se conectou e obteve IP com sucesso, false caso contrario.
 */
bool wifi_init_sta(void);

