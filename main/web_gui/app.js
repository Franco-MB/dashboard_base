// =========================================================================
// app.js — Comunicacao WebSocket (/ws) entre o Navegador e o ESP32 (ou Docker)
//
// COMO FUNCIONA:
//   1. Conecta automaticamente em ws://<IP_OU_LOCALHOST>/ws
//   2. Para ENVIAR dados ao ESP32, basta chamar:
//        enviar("nome_do_tipo", valor)
//      Isso envia o JSON: {"tipo": "nome_do_tipo", "valor": valor}
//   3. Para RECEBER dados do ESP32, tratamos em ws.onmessage dentro do
//      switch (msg.tipo).
// =========================================================================

document.addEventListener("DOMContentLoaded", () => {

    // =====================================================================
    // 1. REFERENCIAS AOS ELEMENTOS DO HTML
    // =====================================================================
    const statusPonto    = document.getElementById("status-ponto");
    const statusTexto    = document.getElementById("status-texto");
    const wsUrlLabel     = document.getElementById("ws-url");

    const caixaOla       = document.getElementById("caixa-ola");
    const barraProgresso = document.getElementById("barra-progresso");
    const progressoValor = document.getElementById("progresso-valor");

    const botoes         = document.querySelectorAll(".btn-acao");
    const checkbox       = document.getElementById("checkbox-exemplo");
    const slider         = document.getElementById("slider-exemplo");
    const sliderValor    = document.getElementById("slider-valor");
    const seletorCor     = document.getElementById("cor-exemplo");
    const corValor       = document.getElementById("cor-valor");
    const dropdown       = document.getElementById("dropdown-exemplo");
    const logMensagens   = document.getElementById("log-mensagens");

    // =====================================================================
    // 2. CONEXAO WEBSOCKET (Funciona igual no Docker e no ESP32)
    // =====================================================================
    let ws = null;

    function conectarWebSocket() {
        // Descobre sozinho se esta rodando em localhost:5173 (Docker) ou no IP do ESP32
        const protocolo = window.location.protocol === "https:" ? "wss:" : "ws:";
        const url = `${protocolo}//${window.location.host}/ws`;
        wsUrlLabel.textContent = url;

        ws = new WebSocket(url);

        // Quando conecta com sucesso
        ws.onopen = () => {
            statusPonto.className = "ponto conectado";
            statusTexto.textContent = "Conectado";
        };

        // Quando a conexao cai -> tenta reconectar sozinho apos 2 segundos
        ws.onclose = () => {
            statusPonto.className = "ponto desconectado";
            statusTexto.textContent = "Reconectando...";
            setTimeout(conectarWebSocket, 2000);
        };

        ws.onerror = () => {
            ws.close();
        };

        // =================================================================
        // 3. RECEBENDO MENSAGENS DO ESP32 (ESP32 -> Navegador)
        // =================================================================
        ws.onmessage = (evento) => {
            let msg;
            try {
                msg = JSON.parse(evento.data);
            } catch (e) {
                console.warn("Mensagem recebida nao e JSON:", evento.data);
                return;
            }

            switch (msg.tipo) {
                // Mensagem enviada pelo ESP32 a cada 1 segundo:
                // {"tipo":"ola", "texto":"ola 1", "contador":1, "progresso":1}
                case "ola":
                    caixaOla.value = msg.texto;
                    barraProgresso.value = msg.progresso;
                    progressoValor.textContent = `${msg.progresso}%`;
                    break;

                // Usado apenas pelo Docker para atualizar a pagina quando voce salva arquivos
                case "dev_reload":
                    if (msg.file && msg.file.endsWith(".css")) {
                        const link = document.querySelector('link[rel="stylesheet"]');
                        if (link) link.href = `/style.css?t=${Date.now()}`;
                    } else {
                        window.location.reload();
                    }
                    break;

                default:
                    console.log("Mensagem recebida do servidor:", msg);
                    break;
            }
        };
    }

    // =====================================================================
    // 4. FUNCAO UNICA PARA ENVIAR DADOS AO ESP32 (Navegador -> ESP32)
    // =====================================================================
    function enviar(tipo, valor) {
        const pacote = { tipo: tipo, valor: valor };
        const jsonTexto = JSON.stringify(pacote);

        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(jsonTexto);
            registrarNoLog(`Enviado -> ${jsonTexto}`);
        } else {
            registrarNoLog(`[Sem conexao WS] Tentou enviar -> ${jsonTexto}`);
        }
    }

    // Deixa a funcao enviar() disponivel no Console do navegador (F12)
    // Assim voce pode testar digitando: enviar("botao", "1")
    window.enviar = enviar;

    function registrarNoLog(linha) {
        const hora = new Date().toLocaleTimeString();
        const textoAtual = logMensagens.textContent.startsWith("Nenhum")
            ? ""
            : logMensagens.textContent + "\n";
        logMensagens.textContent = `${textoAtual}[${hora}] ${linha}`;
        logMensagens.scrollTop = logMensagens.scrollHeight;
    }

    // =====================================================================
    // 5. EVENTOS DOS CONTROLES (Cliques e mudancas)
    // =====================================================================

    // 5.1 Botoes (Botao 1, Botao 2, Botao 3)
    botoes.forEach((botao) => {
        botao.addEventListener("click", () => {
            const numeroBotao = botao.getAttribute("data-valor");
            enviar("botao", numeroBotao);
        });
    });

    // 5.2 Caixa de selecao (Checkbox)
    checkbox.addEventListener("change", () => {
        enviar("checkbox", checkbox.checked);
    });

    // 5.3 Slider (atualiza o numero na tela enquanto arrasta e envia ao soltar)
    slider.addEventListener("input", () => {
        sliderValor.textContent = slider.value;
    });
    slider.addEventListener("change", () => {
        enviar("slider", Number(slider.value));
    });

    // 5.4 Seletor de cor (mostra o codigo HEX e envia ao escolher a cor)
    seletorCor.addEventListener("input", () => {
        corValor.textContent = seletorCor.value;
    });
    seletorCor.addEventListener("change", () => {
        enviar("cor", seletorCor.value);
    });

    // 5.5 Dropdown (Select)
    dropdown.addEventListener("change", () => {
        enviar("dropdown", dropdown.value);
    });

    // Inicia a conexao WebSocket assim que a pagina carrega
    conectarWebSocket();
});
