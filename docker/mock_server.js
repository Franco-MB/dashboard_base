// =========================================================================
// MOCK SERVER (HTTP Estatico + WebSocket /ws + Live-Reload)
// Simula no PC (via Docker) exatamente o mesmo comportamento do ESP32 (main.c)
// =========================================================================

import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocketServer } from "ws";
import chokidar from "chokidar";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = Number(process.env.PORT || 5173);
const WEB_GUI_DIR = process.env.WEB_GUI_DIR
    ? path.resolve(process.env.WEB_GUI_DIR)
    : path.resolve(__dirname, "../main/web_gui");

// =========================================================================
// 1. SERVIDOR HTTP ESTATICO (Serve index.html, style.css e app.js)
// =========================================================================
const MIME_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".json": "application/json; charset=utf-8",
    ".ico": "image/x-icon"
};

const server = http.createServer((req, res) => {
    const cleanUrl = (req.url || "/").split("?")[0];
    const targetFile = cleanUrl === "/" ? "index.html" : cleanUrl.replace(/^\/+/, "");
    const filePath = path.join(WEB_GUI_DIR, path.basename(targetFile));

    fs.readFile(filePath, (err, content) => {
        if (err) {
            res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
            res.end(`404 - Arquivo nao encontrado em ${WEB_GUI_DIR}: ${targetFile}`);
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const contentType = MIME_TYPES[ext] || "application/octet-stream";

        res.writeHead(200, {
            "Content-Type": contentType,
            "Cache-Control": "no-store, no-cache, must-revalidate"
        });
        res.end(content);
    });
});

// =========================================================================
// 2. SERVIDOR WEBSOCKET (/ws) — Imita o main.c do ESP32
// =========================================================================
const wss = new WebSocketServer({ server, path: "/ws" });

let contador = 0;

function enviarParaTodos(objetoJson) {
    const texto = JSON.stringify(objetoJson);
    for (const cliente of wss.clients) {
        if (cliente.readyState === 1) {
            cliente.send(texto);
        }
    }
}

// Simula a funcao tratar_mensagem() do main.c no ESP32
function tratarMensagemDoNavegador(textoRecebido) {
    let msg;
    try {
        msg = JSON.parse(textoRecebido);
    } catch (err) {
        console.warn("[ESP32-MOCK] Mensagem nao e JSON valido:", textoRecebido);
        return;
    }

    const { tipo, valor } = msg;

    switch (tipo) {
        case "botao":
            console.log(`I (MOCK) DASHBOARD: [BOTAO] Botao ${valor} foi clicado!`);
            break;
        case "checkbox":
            console.log(`I (MOCK) DASHBOARD: [CHECKBOX] Alterado para: ${valor ? "MARCADO (true)" : "DESMARCADO (false)"}`);
            break;
        case "slider":
            console.log(`I (MOCK) DASHBOARD: [SLIDER] Novo valor: ${valor}`);
            break;
        case "cor":
            console.log(`I (MOCK) DASHBOARD: [COR] Cor escolhida (HEX): ${valor}`);
            break;
        case "dropdown":
            console.log(`I (MOCK) DASHBOARD: [DROPDOWN] Opcao selecionada: ${valor}`);
            break;
        default:
            console.log(`I (MOCK) DASHBOARD: [OUTRO] tipo='${tipo}' | JSON bruto: ${textoRecebido}`);
            break;
    }
}

wss.on("connection", (ws, req) => {
    const ip = req.socket.remoteAddress;
    console.log(`I (MOCK) DASHBOARD: Navegador conectado no WebSocket (${ip})`);

    // Envia imediatamente o valor atual ao conectar
    ws.send(JSON.stringify({
        tipo: "ola",
        texto: `ola ${contador}`,
        contador: contador,
        progresso: contador % 101
    }));

    ws.on("message", (rawMessage) => {
        tratarMensagemDoNavegador(rawMessage.toString());
    });

    ws.on("close", () => {
        console.log("I (MOCK) DASHBOARD: Navegador desconectado do WebSocket");
    });
});

// Simula a task tarefa_ola() do ESP32: a cada 1 segundo incrementa e envia "ola N"
setInterval(() => {
    contador++;
    const progresso = contador % 101;
    enviarParaTodos({
        tipo: "ola",
        texto: `ola ${contador}`,
        contador: contador,
        progresso: progresso
    });
}, 1000);

// =========================================================================
// 3. LIVE-RELOAD: Atualiza o navegador automaticamente ao salvar arquivos
// =========================================================================
const usePolling = process.env.CHOKIDAR_USEPOLLING === "true";
const watcher = chokidar.watch(WEB_GUI_DIR, {
    ignoreInitial: true,
    usePolling,
    interval: 250
});

watcher.on("change", (filePath) => {
    const fileName = path.basename(filePath);
    console.log(`[Live-Reload] Arquivo salvo: ${fileName} -> Atualizando navegador...`);
    enviarParaTodos({
        tipo: "dev_reload",
        file: fileName
    });
});

// =========================================================================
// 4. INICIALIZACAO DO SERVIDOR
// =========================================================================
server.listen(PORT, "0.0.0.0", () => {
    console.log("=============================================================");
    console.log(` 🚀 Mock ESP32 (Docker) ativo na porta ${PORT}`);
    console.log(` 📂 Servindo arquivos de: ${WEB_GUI_DIR}`);
    console.log(` 🌐 Abra no navegador:    http://localhost:${PORT}`);
    console.log(` 🔌 Endpoint WebSocket:   ws://localhost:${PORT}/ws`);
    console.log("=============================================================");
});
