/*
 * ============================================================================
 * DASHBOARD BASE SIMPLIFICADO — ESP32 + HTTP Server + WebSocket (/ws)
 * Alvo: ESP-IDF v5.5.x (C Puro)
 *
 * O QUE ESTE ARQUIVO FAZ:
 *   1. Conecta no Wi-Fi usando wifi_init_sta() (configurado no menuconfig).
 *   2. Serve os 3 arquivos da pasta main/web_gui/ (index.html, style.css, app.js).
 *   3. Abre o WebSocket em /ws:
 *      - Tudo que o navegador envia chega em tratar_mensagem() e vai para o LOG.
 *      - A cada 1 segundo, a task tarefa_ola() envia para o navegador:
 *        {"tipo":"ola","texto":"ola N","contador":N,"progresso":P}
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "wifi_station.h"

static const char *TAG = "DASHBOARD";

/* Handle global do servidor HTTP/WebSocket */
static httpd_handle_t g_server = NULL;

/* ============================================================================
 * 1. ARQUIVOS DA PASTA main/web_gui/ EMBUTIDOS NA FLASH PELO CMAKE
 * (Nao precisa alterar: o CMake gera esses ponteiros automaticamente)
 * ============================================================================ */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");

extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");

/* ============================================================================
 * 2. ROTAS HTTP GET PARA SERVIR OS 3 ARQUIVOS AO NAVEGADOR
 * ============================================================================ */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)index_html_start,
                           index_html_end - index_html_start);
}

static esp_err_t style_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    return httpd_resp_send(req, (const char *)style_css_start,
                           style_css_end - style_css_start);
}

static esp_err_t app_js_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    return httpd_resp_send(req, (const char *)app_js_start,
                           app_js_end - app_js_start);
}

/* ============================================================================
 * 3. ONDE VOCE TRATA OS DADOS QUE VEM DO NAVEGADOR PELO WEBSOCKET
 *
 * Toda mensagem enviada pela funcao enviar(tipo, valor) do app.js chega aqui
 * no formato JSON: {"tipo": "...", "valor": ...}
 *
 * PARA CRIAR UM CONTROLE NOVO:
 *   Basta adicionar mais um "else if (strcmp(tipo, "seu_tipo") == 0)" abaixo!
 * ============================================================================ */
static void tratar_mensagem(const char *json_recebido)
{
    cJSON *root = cJSON_Parse(json_recebido);
    if (root == NULL) {
        ESP_LOGW(TAG, "Mensagem recebida nao e um JSON valido: %s", json_recebido);
        return;
    }

    cJSON *item_tipo  = cJSON_GetObjectItem(root, "tipo");
    cJSON *item_valor = cJSON_GetObjectItem(root, "valor");

    if (!cJSON_IsString(item_tipo)) {
        ESP_LOGW(TAG, "JSON sem campo 'tipo': %s", json_recebido);
        cJSON_Delete(root);
        return;
    }

    const char *tipo = item_tipo->valuestring;

    /* --- 1. Botoes: {"tipo":"botao","valor":"1"} --- */
    if (strcmp(tipo, "botao") == 0 && cJSON_IsString(item_valor)) {
        ESP_LOGI(TAG, "[BOTAO] Botao %s foi clicado!", item_valor->valuestring);
        // TODO: coloque sua acao aqui (ex: gpio_set_level(GPIO_NUM_2, 1);)
    }
    /* --- 2. Caixa de selecao (Checkbox): {"tipo":"checkbox","valor":true} --- */
    else if (strcmp(tipo, "checkbox") == 0 && cJSON_IsBool(item_valor)) {
        bool marcado = cJSON_IsTrue(item_valor);
        ESP_LOGI(TAG, "[CHECKBOX] Alterado para: %s", marcado ? "MARCADO (true)" : "DESMARCADO (false)");
        // TODO: sua acao aqui
    }
    /* --- 3. Slider: {"tipo":"slider","valor":42} --- */
    else if (strcmp(tipo, "slider") == 0 && cJSON_IsNumber(item_valor)) {
        int nivel = item_valor->valueint;
        ESP_LOGI(TAG, "[SLIDER] Novo valor: %d", nivel);
        // TODO: sua acao aqui (ex: controlar PWM / brilho de LED)
    }
    /* --- 4. Seletor de cor: {"tipo":"cor","valor":"#ff8800"} --- */
    else if (strcmp(tipo, "cor") == 0 && cJSON_IsString(item_valor)) {
        ESP_LOGI(TAG, "[COR] Cor escolhida (HEX): %s", item_valor->valuestring);
        // TODO: sua acao aqui (ex: converter HEX para RGB e aplicar em LED enderecavel)
    }
    /* --- 5. Dropdown (Select): {"tipo":"dropdown","valor":"opcao2"} --- */
    else if (strcmp(tipo, "dropdown") == 0 && cJSON_IsString(item_valor)) {
        ESP_LOGI(TAG, "[DROPDOWN] Opcao selecionada: %s", item_valor->valuestring);
        // TODO: sua acao aqui
    }
    /* --- Qualquer outro tipo novo que voce testar --- */
    else {
        ESP_LOGI(TAG, "[OUTRO] tipo='%s' | JSON bruto: %s", tipo, json_recebido);
    }

    cJSON_Delete(root);
}

/* ============================================================================
 * 4. HANDLER DO WEBSOCKET (/ws) — Recebe os pacotes do navegador
 * ============================================================================ */
static esp_err_t ws_handler(httpd_req_t *req)
{
    /* Quando o navegador abre a conexao ws://.../ws (handshake inicial GET) */
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Navegador conectado no WebSocket (fd=%d)", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    /* Passo A: descobre o tamanho da mensagem que chegou (len) */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK || ws_pkt.len == 0) {
        return ret;
    }

    /* Passo B: aloca memoria, le o texto completo e chama tratar_mensagem() */
    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret == ESP_OK) {
        tratar_mensagem((const char *)ws_pkt.payload);
    }

    free(buf);
    return ret;
}

/* ============================================================================
 * 5. ENVIO DO ESP32 PARA O NAVEGADOR (Broadcast a cada 1 segundo)
 *
 * Envia para todos os navegadores conectados:
 *   {"tipo":"ola","texto":"ola 1","contador":1,"progresso":1}
 *
 * DICA: Para enviar outros dados do ESP32 para a pagina (ex: leitura de sensor),
 * basta adicionar campos com cJSON_AddNumberToObject / cJSON_AddStringToObject
 * ou criar uma funcao parecida com enviar_para_todos_ws(json_str).
 * ============================================================================ */
static void enviar_para_todos_ws(const char *mensagem_json)
{
    if (g_server == NULL) {
        return;
    }

    size_t max_clientes = 8;
    int client_fds[8];
    if (httpd_get_client_list(g_server, &max_clientes, client_fds) != ESP_OK) {
        return;
    }

    httpd_ws_frame_t ws_pkt = {
        .final      = true,
        .fragmented = false,
        .type       = HTTPD_WS_TYPE_TEXT,
        .payload    = (uint8_t *)mensagem_json,
        .len        = strlen(mensagem_json),
    };

    for (size_t i = 0; i < max_clientes; i++) {
        int fd = client_fds[i];
        if (httpd_ws_get_fd_info(g_server, fd) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(g_server, fd, &ws_pkt);
        }
    }
}

static void tarefa_ola(void *pvParameters)
{
    int contador = 0;
    char texto_ola[32];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); /* Espera 1 segundo */

        contador++;
        int progresso = contador % 101; /* Vai de 0 a 100 e reinicia */
        snprintf(texto_ola, sizeof(texto_ola), "ola %d", contador);

        /* Monta o JSON: {"tipo":"ola","texto":"ola N","contador":N,"progresso":P} */
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "tipo", "ola");
        cJSON_AddStringToObject(root, "texto", texto_ola);
        cJSON_AddNumberToObject(root, "contador", contador);
        cJSON_AddNumberToObject(root, "progresso", progresso);

        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str != NULL) {
            enviar_para_todos_ws(json_str);
            free(json_str);
        }
        cJSON_Delete(root);
    }
}

/* ============================================================================
 * 6. INICIALIZACAO DO SERVIDOR HTTP + WEBSOCKET
 * ============================================================================ */
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    if (httpd_start(&g_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar o servidor HTTP!");
        return;
    }

    const httpd_uri_t index_uri = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = index_get_handler,
    };
    const httpd_uri_t style_uri = {
        .uri     = "/style.css",
        .method  = HTTP_GET,
        .handler = style_get_handler,
    };
    const httpd_uri_t app_js_uri = {
        .uri     = "/app.js",
        .method  = HTTP_GET,
        .handler = app_js_get_handler,
    };
    const httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };

    httpd_register_uri_handler(g_server, &index_uri);
    httpd_register_uri_handler(g_server, &style_uri);
    httpd_register_uri_handler(g_server, &app_js_uri);
    httpd_register_uri_handler(g_server, &ws_uri);
}

/* ============================================================================
 * 7. PONTO DE ENTRADA (app_main)
 * ============================================================================ */
void app_main(void)
{
    /* 1. Inicializa NVS (usado pelo driver Wi-Fi para calibracao/config) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Inicializa a pilha de rede (TCP/IP) e o loop de eventos padrao */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 3. Conecta ao Wi-Fi usando o modulo wifi_station (exemplo oficial) */
    wifi_init_sta();

    /* 4. Inicia o servidor HTTP + WebSocket e a tarefa que envia "ola N" */
    start_webserver();
    xTaskCreate(tarefa_ola, "tarefa_ola", 4096, NULL, 5, NULL);
}
