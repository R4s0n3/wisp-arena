const PORT = Number.parseInt(process.env.PORT || "9001", 10);
const HOST = process.env.HOST || "0.0.0.0";
const MAX = 6; // 3v3

let lobbies = [];
let nextId = 1;
const wsInfo = new WeakMap();

function findLobby() {
  let l = lobbies.find((l) => l.players.size < MAX);
  if (!l) {
    l = { id: lobbies.length ? lobbies[lobbies.length - 1].id + 1 : 0, players: new Map() };
    lobbies.push(l);
  }
  return l;
}

if (!Number.isInteger(PORT) || PORT <= 0 || PORT > 65535) {
  throw new Error(`Invalid PORT: ${process.env.PORT}`);
}

const server = Bun.serve({
  hostname: HOST,
  port: PORT,

  fetch(req, server) {
    if (server.upgrade(req)) return;

    return new Response("Wisp Arena websocket server\n", {
      status: 426,
      headers: {
        "Content-Type": "text/plain; charset=utf-8",
        "Upgrade": "websocket",
      },
    });
  },

  websocket: {
    idleTimeout: 60,

    open(ws) {
      const lobby = findLobby();
      const counts = [0, 0];
      for (const p of lobby.players.values()) counts[p.team]++;
      const team = counts[0] <= counts[1] ? 0 : 1;
      const id = nextId++;

      wsInfo.set(ws, { id, team, lobby });
      lobby.players.set(ws, { id, team });
      ws.subscribe("lobby" + lobby.id);
      ws.send(`W|${id}|${team}`);
      console.log(`player ${id} joined lobby ${lobby.id} (team ${team})`);
    },

    message(ws, msg) {
      const s = typeof msg === "string" ? msg : Buffer.from(msg).toString();
      const { id, team, lobby } = wsInfo.get(ws);
      const topic = "lobby" + lobby.id;
      const p = s.split("|");

      if (p[0] === "P") ws.publish(topic, `P|${id}|${p[1]}|${p[2]}|${team}`);
      else if (p[0] === "F")
        ws.publish(topic, `F|${id}|${p[1]}|${p[2]}|${p[3]}|${p[4]}|${team}`);
      else if (p[0] === "H") ws.publish(topic, `H|${p[1]}|${p[2]}`);
    },

    close(ws) {
      const { id, lobby } = wsInfo.get(ws) || {};
      if (!lobby) return;
      lobby.players.delete(ws);
      server.publish("lobby" + lobby.id, `L|${id}`);
      if (lobby.players.size === 0) lobbies = lobbies.filter((l) => l !== lobby);
      console.log(`player ${id} left`);
    },
  },
});

console.log(`Wisp Arena server on ${server.hostname}:${server.port}`);
