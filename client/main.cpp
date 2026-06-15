#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#endif

#include "raylib.h"
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

static const int SW = 1280, SH = 720;
static const int MAX_PARTICLES = 4096;
static const char *DEFAULT_SERVER_URL = "ws://localhost:9001";
static const float PLAYER_RADIUS = 18.0f;
static const float MOVE_SPEED = 320.0f;
static const float BULLET_SPEED = 600.0f;
static const float SUPER_BULLET_SPEED = 480.0f;
static const int BULLET_DAMAGE = 25;
static const int SUPER_DAMAGE = 55;
static const float SUPER_COOLDOWN = 6.0f;
static const float DASH_COOLDOWN = 1.1f;
static const float DASH_SPEED = 980.0f;
static const float DASH_DRAG = 8.0f;
static const float REMOTE_SMOOTHING = 18.0f;

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
struct Remote {
  Vector2 pos{0, 0};
  Vector2 target{0, 0};
  Vector2 dashVel{0, 0};
  int team = 0;
  bool seen = false;
};
struct Bullet {
  Vector2 pos, vel;
  int owner, team, damage;
  float life, radius;
  bool super;
};

static Vector2 add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
static Vector2 scale(Vector2 v, float s) { return {v.x * s, v.y * s}; }
static float length(Vector2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

static Vector2 normalized(Vector2 v) {
  float len = length(v);
  return len > 0.001f ? scale(v, 1.0f / len) : Vector2{0, 0};
}

static Vector2 clampToArena(Vector2 p) {
  p.x = std::clamp(p.x, PLAYER_RADIUS, (float)SW - PLAYER_RADIUS);
  p.y = std::clamp(p.y, PLAYER_RADIUS, (float)SH - PLAYER_RADIUS);
  return p;
}

static float approachZero(float value, float amount) {
  if (value > 0) return std::max(0.0f, value - amount);
  if (value < 0) return std::min(0.0f, value + amount);
  return 0;
}

static Vector2 decayVelocity(Vector2 v, float dt) {
  float amount = DASH_SPEED * DASH_DRAG * dt;
  v.x = approachZero(v.x, amount);
  v.y = approachZero(v.y, amount);
  return v;
}

static Bullet makeBullet(Vector2 pos, Vector2 vel, int owner, int team,
                         bool super) {
  return {pos, vel, owner, team, super ? SUPER_DAMAGE : BULLET_DAMAGE, 2.0f,
          super ? 11.0f : 5.0f, super};
}

static void drawWisp(Vector2 p, Color c) {
  BeginBlendMode(BLEND_ADDITIVE);
  DrawCircleV(p, 18, Fade(c, 0.25f));
  DrawCircleV(p, 10, Fade(c, 0.5f));
  DrawCircleV(p, 5, WHITE);
  EndBlendMode();
}

static std::string trim(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                        s.back() == ' ' || s.back() == '\t')) {
    s.pop_back();
  }
  size_t first = 0;
  while (first < s.size() &&
         (s[first] == '\r' || s[first] == '\n' || s[first] == ' ' ||
          s[first] == '\t')) {
    ++first;
  }
  return s.substr(first);
}

static std::string readFirstLine(const std::string &path) {
  std::ifstream file(path);
  std::string line;
  if (!file || !std::getline(file, line)) return "";
  return trim(line);
}

static std::string executableDir() {
#ifdef _WIN32
  char path[MAX_PATH] = {};
  DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) return "";
  std::string full(path, len);
  size_t slash = full.find_last_of("\\/");
  return slash == std::string::npos ? "" : full.substr(0, slash);
#else
  return "";
#endif
}

static std::string resolveServerUrl(int argc, char **argv) {
  if (argc > 1 && argv[1][0] != '\0') return argv[1];

  if (const char *env = std::getenv("WISP_SERVER_URL")) {
    if (env[0] != '\0') return env;
  }

  std::string url = readFirstLine("server-url.txt");
  if (!url.empty()) return url;

  std::string dir = executableDir();
  if (!dir.empty()) {
    url = readFirstLine(dir + "\\server-url.txt");
    if (!url.empty()) return url;
  }

  return DEFAULT_SERVER_URL;
}

int main(int argc, char **argv) {
  std::string url = resolveServerUrl(argc, argv);

  // --- networking ---
  ix::initNetSystem();
  std::mutex mtx;
  std::vector<std::string> inbox;
  std::string connectionStatus = "connecting...";
  std::string connectionDetail = url;
  ix::WebSocket sock;
  sock.setUrl(url);
  sock.setOnMessageCallback([&](const ix::WebSocketMessagePtr &m) {
    std::lock_guard<std::mutex> l(mtx);
    if (m->type == ix::WebSocketMessageType::Open) {
      connectionStatus = "websocket open, waiting for server...";
      connectionDetail = url;
    } else if (m->type == ix::WebSocketMessageType::Message) {
      inbox.push_back(m->str);
    } else if (m->type == ix::WebSocketMessageType::Error) {
      connectionStatus = "connection error";
      connectionDetail = m->errorInfo.reason;
    } else if (m->type == ix::WebSocketMessageType::Close) {
      connectionStatus = "connection closed";
      connectionDetail = m->closeInfo.reason.empty() ? url : m->closeInfo.reason;
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
  Vector2 dashVel = {0, 0};
  Vector2 lastAim = {1, 0};
  float sendTimer = 0, shootCd = 0, superCd = 0, dashCd = 0;

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
            connectionStatus = "connected";
            connectionDetail = url;
          }
          break;
        case 'P':
          if (sscanf(s.c_str(), "P|%d|%f|%f|%d", &id, &x, &y, &team) == 4) {
            if (id == myId) break;
            Remote &r = remotes[id];
            r.team = team;
            r.target = {x, y};
            if (!r.seen) {
              r.pos = r.target;
              r.seen = true;
            }
          }
          break;
        case 'F':
          if (sscanf(s.c_str(), "F|%d|%f|%f|%f|%f|%d", &id, &x, &y, &dx, &dy,
                     &team) == 6 && id != myId)
            bullets.push_back(makeBullet({x, y}, {dx, dy}, id, team, false));
          break;
        case 'S':
          if (sscanf(s.c_str(), "S|%d|%f|%f|%f|%f|%d", &id, &x, &y, &dx, &dy,
                     &team) == 6 && id != myId) {
            bullets.push_back(makeBullet({x, y}, {dx, dy}, id, team, true));
            fx.burst({x, y}, 10, 180, teamColor(team));
          }
          break;
        case 'D':
          if (sscanf(s.c_str(), "D|%d|%f|%f|%f|%f|%d", &id, &x, &y, &dx, &dy,
                     &team) == 6 && id != myId) {
            Remote &r = remotes[id];
            r.team = team;
            r.pos = {x, y};
            r.target = {x, y};
            r.dashVel = {dx, dy};
            r.seen = true;
            fx.burst(r.pos, 12, 220, teamColor(team));
          }
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
    Vector2 moveDir = normalized(dir);
    if (moveDir.x != 0 || moveDir.y != 0) {
      lastAim = moveDir;
      me = add(me, scale(moveDir, MOVE_SPEED * dt));
    }
    dashCd -= dt;
    bool dashPressed = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT);
    if (dashCd <= 0 && dashPressed && myId >= 0) {
      Vector2 dashDir = moveDir;
      if (dashDir.x == 0 && dashDir.y == 0) dashDir = lastAim;
      dashVel = scale(normalized(dashDir), DASH_SPEED);
      char buf[96];
      snprintf(buf, sizeof buf, "D|%.1f|%.1f|%.1f|%.1f", me.x, me.y,
               dashVel.x, dashVel.y);
      sock.send(buf);
      fx.burst(me, 16, 280, teamColor(myTeam));
      dashCd = DASH_COOLDOWN;
    }
    me = add(me, scale(dashVel, dt));
    dashVel = decayVelocity(dashVel, dt);
    me = clampToArena(me);

    for (auto &[rid, r] : remotes) {
      r.target = clampToArena(r.target);
      r.pos = add(r.pos, scale(r.dashVel, dt));
      float blend = 1.0f - expf(-REMOTE_SMOOTHING * dt);
      r.pos.x += (r.target.x - r.pos.x) * blend;
      r.pos.y += (r.target.y - r.pos.y) * blend;
      r.pos = clampToArena(r.pos);
      r.dashVel = decayVelocity(r.dashVel, dt);
    }

    // --- shooting ---
    shootCd -= dt;
    superCd -= dt;
    if (shootCd <= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && myId >= 0) {
      Vector2 m = GetMousePosition();
      float dx = m.x - me.x, dy = m.y - me.y;
      Vector2 shotDir = normalized({dx, dy});
      if (shotDir.x != 0 || shotDir.y != 0) {
        dx = shotDir.x * BULLET_SPEED;
        dy = shotDir.y * BULLET_SPEED;
        bullets.push_back(makeBullet(me, {dx, dy}, myId, myTeam, false));
        char buf[96];
        snprintf(buf, sizeof buf, "F|%.1f|%.1f|%.1f|%.1f", me.x, me.y, dx, dy);
        sock.send(buf);
        shootCd = 0.2f;
      }
    }
    if (superCd <= 0 && IsKeyPressed(KEY_E) && myId >= 0) {
      Vector2 m = GetMousePosition();
      Vector2 shotDir = normalized({m.x - me.x, m.y - me.y});
      if (shotDir.x != 0 || shotDir.y != 0) {
        Vector2 vel = scale(shotDir, SUPER_BULLET_SPEED);
        bullets.push_back(makeBullet(me, vel, myId, myTeam, true));
        char buf[96];
        snprintf(buf, sizeof buf, "S|%.1f|%.1f|%.1f|%.1f", me.x, me.y, vel.x,
                 vel.y);
        sock.send(buf);
        fx.burst(me, 18, 260, teamColor(myTeam));
        superCd = SUPER_COOLDOWN;
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
          float hitRadius = PLAYER_RADIUS + b.radius;
          if (hx * hx + hy * hy < hitRadius * hitRadius) {
            char buf[48];
            snprintf(buf, sizeof buf, "H|%d|%d", rid, b.damage);
            sock.send(buf);
            fx.burst(b.pos, b.super ? 30 : 15, b.super ? 420 : 300,
                     teamColor(b.team));
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
    for (auto &b : bullets) {
      Color c = teamColor(b.team);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawCircleV(b.pos, b.radius * 2.0f, Fade(c, b.super ? 0.32f : 0.18f));
      DrawCircleV(b.pos, b.radius, Fade(c, b.super ? 0.85f : 0.65f));
      DrawCircleV(b.pos, b.super ? 4.0f : 2.0f, WHITE);
      EndBlendMode();
    }
    for (auto &[rid, r] : remotes) drawWisp(r.pos, teamColor(r.team));
    drawWisp(me, teamColor(myTeam));

    DrawRectangle(20, 20, 200, 14, Color{40, 40, 40, 255});
    DrawRectangle(20, 20, hp * 2, 14, teamColor(myTeam));
    std::string statusText;
    std::string detailText;
    {
      std::lock_guard<std::mutex> l(mtx);
      statusText = connectionStatus;
      detailText = connectionDetail;
    }
    DrawText(myId >= 0 ? TextFormat("ID %d  TEAM %d", myId, myTeam)
                       : statusText.c_str(),
             20, 42, 18, GRAY);
    if (myId >= 0) {
      float superReady = 1.0f - std::clamp(superCd / SUPER_COOLDOWN, 0.0f, 1.0f);
      float dashReady = 1.0f - std::clamp(dashCd / DASH_COOLDOWN, 0.0f, 1.0f);
      DrawText(TextFormat("SUPER %.0f%%", superReady * 100.0f), 20, 64, 14,
               LIGHTGRAY);
      DrawRectangle(100, 66, 90, 8, Color{40, 40, 40, 255});
      DrawRectangle(100, 66, (int)(90 * superReady), 8, Color{235, 225, 90, 255});
      DrawText(TextFormat("DASH %.0f%%", dashReady * 100.0f), 20, 82, 14,
               LIGHTGRAY);
      DrawRectangle(100, 84, 90, 8, Color{40, 40, 40, 255});
      DrawRectangle(100, 84, (int)(90 * dashReady), 8, Color{120, 245, 210, 255});
    }
    if (myId < 0 && !detailText.empty()) {
      DrawText(detailText.c_str(), 20, 64, 14, DARKGRAY);
    }
    EndDrawing();
  }

  sock.stop();
  CloseWindow();
  ix::uninitNetSystem();
  return 0;
}
