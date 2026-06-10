const uWS = require("uWebSockets.js");
const PORT = 9001;
const MAX = 6; // 3v3

let lobbies = [];
let nextId = 1;

function findLobby() {
  let l = lobbies.find((l) => l.players.size < MAX);
  if (!l) {
    l = { id: lobbies.length ? lobbies[lobbies.length - 1].id + 1 : 0, players: new Map() };
    lobbies.push(l);
  }
  return l;
}

const app = uWS
  .App()
  .ws("/*", {
    idleTimeout: 60,

    open: (ws) => {
      const lobby = findLobby();
      const counts = [0, 0];
      for (const p of lobby.players.values()) counts[p.team]++;
      const team = counts[0] <= counts[1] ? 0 : 1;
      const id = nextId++;

      ws.info = { id, team, lobby };
      lobby.players.set(ws, { id, team });
      ws.subscribe("lobby" + lobby.id);
      ws.send(`W|${id}|${team}`);
      console.log(`player ${id} joined lobby ${lobby.id} (team ${team})`);
    },

    message: (ws, msg) => {
      const s = Buffer.from(msg).toString();
      const { id, team, lobby } = ws.info;
      const topic = "lobby" + lobby.id;
      const p = s.split("|");

      if (p[0] === "P") ws.publish(topic, `P|${id}|${p[1]}|${p[2]}|${team}`);
      else if (p[0] === "F")
        ws.publish(topic, `F|${id}|${p[1]}|${p[2]}|${p[3]}|${p[4]}|${team}`);
      else if (p[0] === "H") ws.publish(topic, `H|${p[1]}|${p[2]}`);
    },

    close: (ws) => {
      const { id, lobby } = ws.info || {};
      if (!lobby) return;
      lobby.players.delete(ws);
      app.publish("lobby" + lobby.id, `L|${id}`);
      if (lobby.players.size === 0) lobbies = lobbies.filter((l) => l !== lobby);
      console.log(`player ${id} left`);
    },
  })
  .listen(PORT, (ok) =>
    console.log(ok ? `Wisp Arena server on :${PORT}` : "Failed to listen")
  );
