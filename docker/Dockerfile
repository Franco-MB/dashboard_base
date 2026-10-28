# Imagem leve Node 22 Alpine para o servidor HTTP + Mock WebSocket (/ws) + Live-Reload
FROM node:22-alpine

WORKDIR /app

# Copia os manifestos para aproveitar o cache de camadas do Docker
COPY package*.json ./

# Instala dependências ('ws' e 'chokidar')
RUN npm install

# Porta padrão do ambiente de desenvolvimento
EXPOSE 5173

# Inicia o servidor Mock + Live-Reload
CMD ["npm", "run", "dev"]
