# Dashboard Base Simplificado — ESP32 + WebSocket (`/ws`)

![Status](https://img.shields.io/badge/status-em%20desenvolvimento-blue)
![Versão](https://img.shields.io/badge/version-1.0.0-green)
![Licença](https://img.shields.io/badge/license-MIT-yellow)

Projeto base **mínimo e didático** para você criar seus próprios painéis web (HTML + CSS + JS) e conversar em tempo real com o ESP32 via **WebSocket**, usando **ESP-IDF v5.5.x** em C puro e um ambiente **Docker** para desenhar e testar tudo no PC antes de gravar na placa.

![Dashboard Base ESP32](.github/assets/dashboard.png)

> **Créditos:** a conexão Wi-Fi (`wifi_station.c`) foi adaptada do exemplo oficial
> [`wifi/getting_started/station`](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station)
> da Espressif. Usa o [ESP-IDF](https://github.com/espressif/esp-idf), o [cJSON](https://github.com/DaveGamble/cJSON) e, no mock, [ws](https://github.com/websockets/ws) e [chokidar](https://github.com/paulmillr/chokidar).

## Índice

1. [Estrutura de Diretórios](#1-estrutura-de-diretórios)
2. [Como Funciona a Comunicação WebSocket (`/ws`)](#2-como-funciona-a-comunicação-websocket-ws)
   - [2.1 Do Navegador para o ESP32](#21-do-navegador-para-o-esp32-enviartipo-valor-no-appjs)
   - [2.2 Do ESP32 para o Navegador](#22-do-esp32-para-o-navegador-tarefa_ola-no-mainc)
3. [Passo a Passo: Criando e Testando no PC com Docker](#3-passo-a-passo-criando-e-testando-no-pc-com-docker-sem-o-esp32)
4. [Passo a Passo: Gravando no ESP32](#4-passo-a-passo-gravando-no-esp32-esp-idf-v55x)
5. [Guia Prático: Como Criar Seus Próprios Controles](#5-guia-prático-como-criar-seus-próprios-controles)
6. [Segurança](#6-segurança)
7. [Licença](#7-licença)

---

## 1. Estrutura de Diretórios

```text
dashboard_base/
├── .github/assets/            # Imagens usadas no README
├── .gitignore                 # Ignora build/, sdkconfig (contém a senha do Wi-Fi), .vscode/...
├── CMakeLists.txt             # CMake raiz do projeto ESP-IDF
├── LICENSE                    # Licença MIT
├── sdkconfig.defaults         # Ativa suporte a WebSocket no servidor HTTP do ESP-IDF
├── README.md                  # Este guia passo a passo
├── main/
│   ├── CMakeLists.txt         # Compila main.c + wifi_station.c e embute web_gui/ na Flash
│   ├── Kconfig.projbuild      # Cria o menu no `idf.py menuconfig` para SSID e senha do Wi-Fi
│   ├── wifi_station.h         # Cabeçalho do módulo Wi-Fi Station (padrão oficial ESP-IDF)
│   ├── wifi_station.c         # Conexão Wi-Fi baseada no exemplo oficial `getting_started/station`
│   ├── main.c                 # Servidor HTTP + WebSocket (/ws) + JSON (cJSON)
│   └── web_gui/               # Os 3 arquivos do seu dashboard (usados no Docker e no ESP32)
│       ├── index.html         # Estrutura dos controles (botões, slider, cor, etc.)
│       ├── style.css          # Visual e cores
│       └── app.js             # Envio e recebimento de JSON via WebSocket
└── docker/                    # Simulador do ESP32 no PC (com Live-Reload)
    ├── Dockerfile
    ├── docker-compose.yml
    ├── package.json
    ├── mock_server.js         # Simula o main.c do ESP32 em Node.js
    └── README.md
```

---

## 2. Como Funciona a Comunicação WebSocket (`/ws`)

Tanto o `mock_server.js` (no Docker) quanto o `main.c` (no ESP32) usam **exatamente o mesmo protocolo JSON** no endereço `/ws`.

### 2.1 Do Navegador para o ESP32 (`enviar(tipo, valor)` no `app.js`)

Sempre que você interage com um controle na página, o `app.js` chama `enviar(tipo, valor)`, que manda um JSON com duas chaves: `"tipo"` e `"valor"`:

| Controle na Página | Quando dispara | JSON enviado pelo WebSocket | Onde é tratado no ESP32 (`main.c`) |
|---|---|---|---|
| **Botões 1, 2, 3** | Ao clicar (`click`) | `{"tipo":"botao","valor":"1"}` | `strcmp(tipo, "botao") == 0` |
| **Caixa de seleção** | Ao marcar/desmarcar (`change`) | `{"tipo":"checkbox","valor":true}` | `strcmp(tipo, "checkbox") == 0` |
| **Slider (0–100)** | Ao soltar o controle (`change`) | `{"tipo":"slider","valor":50}` | `strcmp(tipo, "slider") == 0` |
| **Seletor de cor** | Ao escolher a cor (`change`) | `{"tipo":"cor","valor":"#38bdf8"}` | `strcmp(tipo, "cor") == 0` |
| **Dropdown** | Ao trocar a opção (`change`) | `{"tipo":"dropdown","valor":"opcao2"}` | `strcmp(tipo, "dropdown") == 0` |

### 2.2 Do ESP32 para o Navegador (`tarefa_ola` no `main.c`)

A cada **1 segundo**, o ESP32 (ou o Mock no Docker) incrementa um contador e envia para todos os navegadores conectados:

```json
{"tipo":"ola","texto":"ola 7","contador":7,"progresso":7}
```

No `app.js`, o bloco `switch (msg.tipo)` captura `case "ola":` e atualiza:
- A **caixa de texto** (`#caixa-ola`) com `"ola 7"`.
- A **barra de progresso** (`#barra-progresso`) com `7` (de 0 a 100%).

---

## 3. Passo a Passo: Criando e Testando no PC com Docker (Sem o ESP32)

1. Suba o container Docker:
   ```bash
   cd docker
   docker compose up -d --build
   ```
2. Abra no navegador: **`http://localhost:5173`**
3. Para ver os logs das mensagens chegando no "ESP32 simulado", rode no terminal:
   ```bash
   docker compose logs -f
   ```
4. **Dica para testar sem criar HTML ainda:** abra o Console do navegador (`F12` &rarr; aba **Console**) e digite:
   ```javascript
   enviar("botao", "1")
   enviar("meu_teste", "qualquer valor")
   ```
   Você verá a mensagem aparecer tanto no quadro de log da página quanto no terminal do `docker compose logs -f`!

---

## 4. Passo a Passo: Gravando no ESP32 (ESP-IDF v5.5.x)

1. **Configure o nome e a senha do seu Wi-Fi (2.4 GHz)** pelo `menuconfig` (igual aos exemplos oficiais da API do ESP-IDF):
   ```bash
   idf.py menuconfig
   ```
   - Entre no menu **`Configuracao do Wi-Fi do Dashboard`**.
   - Preencha **`WiFi SSID`** e **`WiFi Password`**.
   - Pressione `S` para salvar e `Q` para sair.
   *(Nota: os valores padrão são `SEU_WIFI` / `SUA_SENHA`. Configure-os pelo menuconfig; eles ficam salvos no `sdkconfig`, que **não é enviado ao git**. Evite colocar a senha real no `main/Kconfig.projbuild`, pois esse arquivo é versionado.)*

2. **Compile, grave e abra o monitor serial:**
   ```bash
   idf.py build flash monitor
   ```

3. No monitor serial, assim que o ESP32 conectar ao Wi-Fi, ele mostrará o endereço IP:
   ```text
   I (1540) WIFI_STA: =====================================================
   I (1540) WIFI_STA:  Conectado! Abra no navegador: http://192.168.1.105
   I (1540) WIFI_STA: =====================================================
   ```

4. Abra esse endereço no navegador do PC ou do celular (conectado no mesmo Wi-Fi) e clique nos controles para ver os logs no terminal!

---

## 5. Guia Prático: Como Criar Seus Próprios Controles

### Exemplo A: Adicionar um controle novo (Página &rarr; ESP32) em 3 passos

Suponha que você queira criar um **campo de texto com um botão "Enviar Nome"** para mandar um texto ao ESP32:

**Passo 1 — No `main/web_gui/index.html`:** adicione o HTML dentro de `<main class="grade">`:
```html
<section class="cartao">
    <h2>Meu Novo Controle</h2>
    <input type="text" id="meu-campo" placeholder="Digite algo...">
    <button type="button" id="meu-botao">Enviar para o ESP32</button>
</section>
```

**Passo 2 — No `main/web_gui/app.js`:** capture o clique e chame `enviar(tipo, valor)`:
```javascript
const meuCampo = document.getElementById("meu-campo");
const meuBotao = document.getElementById("meu-botao");

meuBotao.addEventListener("click", () => {
    enviar("nome_usuario", meuCampo.value);
});
```

**Passo 3 — No `main/main.c` (dentro de `tratar_mensagem`):** adicione mais um `else if`:
```c
else if (strcmp(tipo, "nome_usuario") == 0 && cJSON_IsString(item_valor)) {
    ESP_LOGI(TAG, "Nome digitado na pagina: %s", item_valor->valuestring);
}
```
Pronto!

---

### Exemplo B: Enviar um dado novo do ESP32 para a Página (ESP32 &rarr; Página)

1. **No `main/main.c`**, monte o JSON com `cJSON` e chame `enviar_para_todos_ws()`:
   ```c
   cJSON *root = cJSON_CreateObject();
   cJSON_AddStringToObject(root, "tipo", "temperatura");
   cJSON_AddNumberToObject(root, "valor", 26.5);
   char *json_str = cJSON_PrintUnformatted(root);
   enviar_para_todos_ws(json_str);
   free(json_str);
   cJSON_Delete(root);
   ```
2. **No `main/web_gui/app.js`**, dentro do `switch (msg.tipo)` em `ws.onmessage`, adicione um `case`:
   ```javascript
   case "temperatura":
       console.log("Temperatura vinda do ESP32:", msg.valor);
       break;
   ```

---

## 6. Segurança

- Sua senha do Wi-Fi fica apenas no `sdkconfig` (ignorado pelo `.gitignore`). Nunca faça commit desse arquivo.
- O WebSocket `/ws` **não tem autenticação**: qualquer dispositivo na mesma rede pode abrir a página e enviar comandos. Use apenas em redes confiáveis ou adicione autenticação antes de controlar algo crítico.

---

## 7. Licença

Distribuído sob a licença MIT. Veja [LICENSE](LICENSE).
