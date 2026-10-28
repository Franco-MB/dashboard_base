# Ambiente Docker de Desenvolvimento (Mock do ESP32)

Esta pasta permite criar, estilizar e testar os 3 arquivos de `../main/web_gui/` (`index.html`, `style.css` e `app.js`) direto no PC, **sem precisar gravar no ESP32 a cada mudança**.

O `mock_server.js` faz exatamente o mesmo papel do `main/main.c` no ESP32:
1. Serve o HTML, CSS e JS em `http://localhost:5173`.
2. Abre o WebSocket em `ws://localhost:5173/ws`.
3. Envia a cada 1 segundo `{"tipo":"ola","texto":"ola N","contador":N,"progresso":P}`.
4. Imprime no log do container tudo o que você clica ou altera na página.
5. Recarrega a página automaticamente toda vez que você salva `index.html`, `style.css` ou `app.js`.

## Comandos Rápidos

Subir o servidor Mock:
```bash
cd docker
docker compose up -d --build
```

Ver o "monitor serial" do Mock em tempo real (mostra cada clique nos botões, slider, cor, etc.):
```bash
docker compose logs -f
```

Parar o servidor:
```bash
docker compose down
```
