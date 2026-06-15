FROM oven/bun:1-alpine

WORKDIR /app

ENV NODE_ENV=production
ENV HOST=0.0.0.0
ENV PORT=9001

COPY server/package.json ./
RUN bun install --production

COPY server/server.js ./

EXPOSE 9001

CMD ["bun", "run", "start"]
