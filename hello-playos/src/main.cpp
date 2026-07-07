// hello-playos — the reference "your first PlayOS app" sample.
//
// Demonstrates the intended developer experience:
//   #include "raylib.h"          // rendering (the reference engine)
//   #include "playos/playos.h"   // platform services (engine-agnostic)
//
// The game runs until the player presses B (or Esc). When it exits, the
// PlayOS Shell regains the foreground — closing the console loop.

#include "raylib.h"

#include "playos/playos.h"

int main() {
    // Fullscreen at native resolution — the same pattern as the shell.
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Hello PlayOS");
    SetTargetFPS(60);

    PlayOS::Lifecycle::Init();

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    Vector2 pos = {width / 2.0f, height / 2.0f};
    Vector2 vel = {220.0f, 160.0f};
    const float radius = 40.0f;

    while (!WindowShouldClose()) {
        // Platform services: poll input through the PlayOS Platform API.
        PlayOS::Lifecycle::Update();

        // B (or Esc) returns to the shell.
        if (PlayOS::Input::Pressed(PlayOS::Button::B) || IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        const float dt = GetFrameTime();
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        if (pos.x < radius || pos.x > width - radius) vel.x = -vel.x;
        if (pos.y < radius || pos.y > height - radius) vel.y = -vel.y;

        BeginDrawing();
        ClearBackground(Color{12, 14, 20, 255});
        DrawText("Hello, PlayOS!", 40, 40, 40, RAYWHITE);
        DrawText("A sample game built with Raylib + the PlayOS Platform API.",
                 40, 92, 20, Color{160, 160, 180, 255});
        DrawText("Press B (or Esc) to return to the shell.",
                 40, 120, 20, Color{160, 160, 180, 255});
        DrawCircleV(pos, radius, Color{40, 120, 220, 255});
        EndDrawing();
    }

    PlayOS::Lifecycle::Shutdown();
    CloseWindow();
    return 0;
}
