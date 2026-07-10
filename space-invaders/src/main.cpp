// space-invaders — a quick Space Invaders clone for PlayOS.
//
// Controls:
//   D-Pad Left/Right or Arrow keys → move
//   A button or Space → shoot
//   B button or Escape → quit (return to shell)

#include "raylib.h"
#include "playos/playos.h"

#include <algorithm>
#include <vector>
#include <cmath>

// ── helpers ────────────────────────────────────────────────────────────
static float Clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
constexpr int kAlienCols = 10;
constexpr int kAlienRows = 4;
constexpr float kAlienW = 40;
constexpr float kAlienH = 30;
constexpr float kAlienPadX = 12;
constexpr float kAlienPadY = 10;
constexpr float kPlayerW = 50;
constexpr float kPlayerH = 20;
constexpr float kBulletW = 4;
constexpr float kBulletH = 12;
constexpr float kPlayerSpeed = 400;
constexpr float kBulletSpeed = 500;
constexpr float kAlienSpeed = 80;
constexpr float kAlienDrop = 20;

struct Alien {
    float x, y;
    bool alive = true;
};

struct Bullet {
    float x, y;
    bool active = false;
};

// ── helpers ──────────────────────────────────────────────────────────────
static bool PressedLeft()  { return PlayOS::Input::Pressed(PlayOS::Button::DPadLeft)  || IsKeyPressed(KEY_LEFT); }
static bool PressedRight() { return PlayOS::Input::Pressed(PlayOS::Button::DPadRight) || IsKeyPressed(KEY_RIGHT); }
static bool PressedFire()  { return PlayOS::Input::Pressed(PlayOS::Button::A)          || IsKeyPressed(KEY_SPACE); }
static bool PressedQuit()  { return PlayOS::Input::Pressed(PlayOS::Button::B)          || IsKeyPressed(KEY_ESCAPE); }
static bool DownLeft()     { return PlayOS::Input::Down(PlayOS::Button::DPadLeft)      || IsKeyDown(KEY_LEFT); }
static bool DownRight()    { return PlayOS::Input::Down(PlayOS::Button::DPadRight)     || IsKeyDown(KEY_RIGHT); }

int main() {
    auto displayInfo = PlayOS::Display::Current();
#ifdef __linux__
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Space Invaders — PlayOS");
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
#else
    const int W = displayInfo.width > 0 ? displayInfo.width : 960;
    const int H = displayInfo.height > 0 ? displayInfo.height : 540;
    InitWindow(W, H, "Space Invaders — PlayOS");
#endif
    SetTargetFPS(displayInfo.refreshRate > 0 ? displayInfo.refreshRate : 60);
    PlayOS::Lifecycle::Init();

    // ── state ────────────────────────────────────────────────────────────
    float playerX = W / 2.0f;
    float alienDir = 1.0f;   // 1 = right, -1 = left
    float alienBaseY = 60;
    int score = 0;
    bool gameOver = false;

    // Build alien grid
    std::vector<Alien> aliens;
    for (int r = 0; r < kAlienRows; ++r) {
        for (int c = 0; c < kAlienCols; ++c) {
            float ax = (W - kAlienCols * (kAlienW + kAlienPadX)) / 2.0f
                       + c * (kAlienW + kAlienPadX);
            float ay = alienBaseY + r * (kAlienH + kAlienPadY);
            aliens.push_back({ax, ay, true});
        }
    }

    std::vector<Bullet> bullets;
    float shootCooldown = 0;

    // ── loop ─────────────────────────────────────────────────────────────
    while (!WindowShouldClose() && !PressedQuit()) {
        float dt = GetFrameTime();
        PlayOS::Lifecycle::Update();

        // ── player ───────────────────────────────────────────────────────
        if (DownLeft())  playerX -= kPlayerSpeed * dt;
        if (DownRight()) playerX += kPlayerSpeed * dt;
        playerX = Clamp(playerX, kPlayerW / 2.0f, W - kPlayerW / 2.0f);

        shootCooldown -= dt;
        if (PressedFire() && shootCooldown <= 0) {
            bullets.push_back({playerX, H - 60, true});
            shootCooldown = 0.25f;
        }

        // ── bullets ──────────────────────────────────────────────────────
        for (auto& b : bullets) {
            if (!b.active) continue;
            b.y -= kBulletSpeed * dt;
            if (b.y < -kBulletH) b.active = false;
        }

        // ── aliens ───────────────────────────────────────────────────────
        bool hitEdge = false;
        for (const auto& a : aliens) {
            if (!a.alive) continue;
            if (a.x < kAlienW / 2 || a.x > W - kAlienW / 2) { hitEdge = true; break; }
        }

        if (hitEdge) {
            alienDir *= -1;
            alienBaseY += kAlienDrop;
        }

        int aliveCount = 0;
        for (auto& a : aliens) {
            if (!a.alive) continue;
            a.x += kAlienSpeed * alienDir * dt;
            a.y = alienBaseY + ((&a - aliens.data()) / kAlienCols) * (kAlienH + kAlienPadY);
            aliveCount++;

            // Game over if aliens reach the player
            if (a.y > H - 120) gameOver = true;

            // Bullet-alien collision
            for (auto& b : bullets) {
                if (!b.active) continue;
                if (CheckCollisionRecs(
                        {a.x - kAlienW/2, a.y - kAlienH/2, kAlienW, kAlienH},
                        {b.x - kBulletW/2, b.y - kBulletH/2, kBulletW, kBulletH})) {
                    a.alive = false;
                    b.active = false;
                    score += 100;
                }
            }
        }

        // Speed up as aliens die
        float speedMul = 1.0f + (float)(kAlienCols * kAlienRows - aliveCount) / (kAlienCols * kAlienRows) * 3.0f;

        // ── draw ─────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(Color{2, 2, 12, 255});

        // Stars
        static float stars[60][2];
        static bool starsInit = false;
        if (!starsInit) {
            for (auto& s : stars) { s[0] = GetRandomValue(0, W); s[1] = GetRandomValue(0, H); }
            starsInit = true;
        }
        for (const auto& s : stars) DrawPixel(s[0], s[1], Color{80, 80, 110, 255});

        // Aliens
        for (const auto& a : aliens) {
            if (!a.alive) continue;
            Color c = (score > 500) ? RED : (score > 200) ? ORANGE : GREEN;
            DrawRectangle(a.x - kAlienW/2, a.y - kAlienH/2, kAlienW, kAlienH, c);
            // "Eyes"
            DrawRectangle(a.x - 8, a.y - 4, 4, 4, BLACK);
            DrawRectangle(a.x + 4, a.y - 4, 4, 4, BLACK);
        }

        // Player
        DrawTriangle(
            {playerX, (float)H - 80},
            {playerX - kPlayerW/2, (float)H - 50},
            {playerX + kPlayerW/2, (float)H - 50},
            Color{60, 180, 240, 255});

        // Bullets
        for (const auto& b : bullets) {
            if (!b.active) continue;
            DrawRectangle(b.x - kBulletW/2, b.y - kBulletH/2, kBulletW, kBulletH, YELLOW);
        }

        // HUD
        DrawText(TextFormat("SCORE: %d", score), 16, 8, 20, RAYWHITE);

        if (gameOver) {
            DrawText("GAME OVER", W/2 - MeasureText("GAME OVER", 40)/2, H/2 - 20, 40, RED);
            DrawText("Press B to quit", W/2 - MeasureText("Press B to quit", 20)/2, H/2 + 30, 20, GRAY);
        }

        if (aliveCount == 0) {
            DrawText("YOU WIN!", W/2 - MeasureText("YOU WIN!", 40)/2, H/2 - 20, 40, GREEN);
        }

        EndDrawing();
    }

    PlayOS::Lifecycle::Shutdown();
    CloseWindow();
    return 0;
}
