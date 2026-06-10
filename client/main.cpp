#include "raylib.h"
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

static const int SW = 1280, SH = 720;
static const int MAX_PARTICLES = 4096;

static float frand(float a, float b) {
  return a + (b - a) * (float)GetRandomValue(0, 1000) / 1000.0f;
}
static Color teamColor(int t) {
  return t == 0 ? Color{80, 200, 255, 255} : Color{255, 150, 60, 255};
}

// ---------- Particle engine ----------
struct Particle {
  Vector2 pos, vel;
  float life, maxLife, size;
  Color color;
};

struct ParticleSystem {
  std::vector<Particle> ps;
  ParticleSystem() { ps.reserve(MAX_PARTICLES); }

  void emit(Vector2 pos, Vector2 vel, float life, float size, Color c) {
    if ((int)ps.size() >= MAX_PARTICLES) return;
    ps.push_back({pos, vel, life, life, size, c});
  }
  void burst(Vector2 pos, int n, float speed, Color c) {
    for (int i = 0; i < n; i++) {
      float a = frand(0, 6.2832f), s = frand(speed * 0.3f, speed);
      emit(pos, {cosf(a) * s, sinf(a) * s}, frand(0.3f, 0.9f), frand(3, 7), c);
    }
  }
  void update(float dt) {
    for (size_t i = 0; i < ps.size();) {
      Particle &p = ps[i];
      p.life -= dt;
      if (p.life <= 0) { ps[i] = ps.back(); ps.pop_back(); continue; }
      p.pos.x += p.vel.x * dt;
      p.pos.y += p.vel.y * dt;
      p.vel.x *= 0.96f;
      p.vel.y *= 0.96f;
      ++i;
    }
  }
  void draw() {
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto &p : ps) {
      float t = p.life / p.maxLife;
      DrawCircleV(p.pos, p.size * t, Fade(p.color, t * 0.7f));
    }
    EndBlendMode();
  }
};

// ---------- Game entities ----------
struct Remote { Vector2 pos{0, 0}; int team = 0; };
struct Bullet { Vector2 pos, vel; int owner, team; float life; };

static void drawWisp(Vector2 p, Color c) {
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleV(p, 18, Fade(c, 0.25f));
  DrawCircleV(p, 10, Fade(c, 0.5f));
  DrawCircleV(p, 5, WHITE);
  EndBlendMode();
}

int main(int argc, char **argv) {
  const char *url = argc > 1 ? argv[1] : "ws://localhost:9001";

  // --- networking ---
  ix::initNetSystem();
  std::mutex mtx;
  std::vector<std::string> inbox;
  ix::WebSocket sock;
  sock.setUrl(url);
  sock.setOnMessageCallback([&](const ix::WebSocketMessagePtr &m) {
    if (m->type == ix::WebSocketMessageType::Message) {
      std::lock_guard<std::mutex> l(mtx);
      inbox.push_back(m->str);
    }
  });
  sock.start();

  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
  InitWindow(SW, SH, "Wisp Arena");

  ParticleSystem fx;
  std::unordered_map<int, Remote> remotes;
  std::vector<Bullet> bullets;

  int myId = -1, myTeam = 0, hp = 100;
  Vector2 me = {frand(100, SW - 100), frand(100, SH - 100)};
  float sendTimer = 0, shootCd = 0;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    // --- process incoming messages ---
    {
      std::lock_guard<std::mutex> l(mtx);
      for (auto &s : inbox) {
        int id, team, dmg;
        float x, y, dx, dy;
        switch (s[0]) {
        case 'W':
          if (sscanf(s.c_str(), "W|%d|%d", &id, &team) == 2) {
            myId = id; myTeam = team;
          }
          break;
        case 'P':
          if (sscanf(s.c_str(), "P|%d|%f|%f|%d", &id, &x, &y, &team) == 4) {
            remotes[id].pos = {x, y};
            remotes[id].team = team;
          }
          break;
        case 'F':
          if (sscanf(s.c_str(), "F|%d|%f|%f|%f|%f|%d", &id, &x, &y, &dx, &dy,
                     &team) == 6)
            bullets.push_back({{x, y}, {dx, dy}, id, team, 2.0f});
          break;
        case 'H':
          if (sscanf(s.c_str(), "H|%d|%d", &id, &dmg) == 2) {
            if (id == myId) {
              hp -= dmg;
              fx.burst(me, 20, 250, teamColor(myTeam));
              if (hp <= 0) { // respawn
                fx.burst(me, 60, 450, WHITE);
                me = {frand(100, SW - 100), frand(100, SH - 100)};
                hp = 100;
              }
            } else if (remotes.count(id)) {
              fx.burst(remotes[id].pos, 20, 250, teamColor(remotes[id].team));
            }
          }
          break;
        case 'L':
          if (sscanf(s.c_str(), "L|%d", &id) == 1) remotes.erase(id);
          break;
        }
      }
      inbox.clear();
    }

    // --- movement ---
    Vector2 dir = {0, 0};
    if (IsKeyDown(KEY_W)) dir.y -= 1;
    if (IsKeyDown(KEY_S)) dir.y += 1;
    if (IsKeyDown(KEY_A)) dir.x -= 1;
    if (IsKeyDown(KEY_D)) dir.x += 1;
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len > 0) {
      me.x += dir.x / len * 320 * dt;
      me.y += dir.y / len * 320 * dt;
    }
    if (me.x < 20) me.x = 20;
    if (me.x > SW - 20) me.x = SW - 20;
    if (me.y < 20) me.y = 20;
    if (me.y > SH - 20) me.y = SH - 20;

    // --- shooting ---
    shootCd -= dt;
    if (shootCd <= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && myId >= 0) {
      Vector2 m = GetMousePosition();
      float dx = m.x - me.x, dy = m.y - me.y;
      float l2 = sqrtf(dx * dx + dy * dy);
      if (l2 > 1) {
        dx = dx / l2 * 600;
        dy = dy / l2 * 600;
        bullets.push_back({me, {dx, dy}, myId, myTeam, 2.0f});
        char buf[96];
        snprintf(buf, sizeof buf, "F|%.1f|%.1f|%.1f|%.1f", me.x, me.y, dx, dy);
        sock.send(buf);
        shootCd = 0.2f;
      }
    }

    // --- bullets ---
    for (size_t i = 0; i < bullets.size();) {
      Bullet &b = bullets[i];
      b.life -= dt;
      b.pos.x += b.vel.x * dt;
      b.pos.y += b.vel.y * dt;
      fx.emit(b.pos, {frand(-30, 30), frand(-30, 30)}, 0.3f, 4,
              teamColor(b.team));

      bool dead = b.life <= 0;
      // shooter-side hit detection (only for my own bullets)
      if (!dead && b.owner == myId) {
        for (auto &[rid, r] : remotes) {
          if (r.team == myTeam) continue;
          float hx = r.pos.x - b.pos.x, hy = r.pos.y - b.pos.y;
          if (hx * hx + hy * hy < 18 * 18) {
            char buf[48];
            snprintf(buf, sizeof buf, "H|%d|%d", rid, 25);
            sock.send(buf);
            fx.burst(b.pos, 15, 300, teamColor(b.team));
            dead = true;
            break;
          }
        }
      }
      if (dead) { bullets[i] = bullets.back(); bullets.pop_back(); }
      else ++i;
    }

    // --- wisp trails ---
    for (int i = 0; i < 3; i++)
      fx.emit({me.x + frand(-6, 6), me.y + frand(-6, 6)},
              {frand(-40, 40), frand(-40, 40)}, 0.6f, 8, teamColor(myTeam));
    for (auto &[rid, r] : remotes)
      fx.emit({r.pos.x + frand(-6, 6), r.pos.y + frand(-6, 6)},
              {frand(-40, 40), frand(-40, 40)}, 0.6f, 8, teamColor(r.team));

    fx.update(dt);

    // --- send position 15 Hz ---
    sendTimer -= dt;
    if (sendTimer <= 0 && myId >= 0) {
      char buf[48];
      snprintf(buf, sizeof buf, "P|%.1f|%.1f", me.x, me.y);
      sock.send(buf);
      sendTimer = 1.0f / 15.0f;
    }

    // --- draw ---
    BeginDrawing();
    ClearBackground(Color{8, 8, 16, 255});
    for (int x = 0; x < SW; x += 80)
      DrawLine(x, 0, x, SH, Color{20, 20, 35, 255});
    for (int y = 0; y < SH; y += 80)
      DrawLine(0, y, SW, y, Color{20, 20, 35, 255});

    fx.draw();
    for (auto &[rid, r] : remotes) drawWisp(r.pos, teamColor(r.team));
    drawWisp(me, teamColor(myTeam));

    DrawRectangle(20, 20, 200, 14, Color{40, 40, 40, 255});
    DrawRectangle(20, 20, hp * 2, 14, teamColor(myTeam));
    DrawText(myId >= 0 ? TextFormat("ID %d  TEAM %d", myId, myTeam)
                       : "connecting...",
             20, 42, 18, GRAY);
    EndDrawing();
  }

  sock.stop();
  CloseWindow();
  ix::uninit
