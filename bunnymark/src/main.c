/*
 * com.playos.sample-bunnymark — PlayOS bunnymark benchmark
 *
 * A PlayOS port of raylib's `textures_bunnymark` example. Spawns up to
 * 80,000 textured bunnies that bounce around the screen, reporting FPS and
 * batched draw calls to measure renderer throughput.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - Input is read through raylib's native gamepad API (the PlayOS raylib
 *     backend exposes a single logical controller fed from libplayos):
 *       A (hold)   — spawn bunnies (upstream left mouse button, 100/frame)
 *       X          — toggle pause (upstream P)
 *       LB         — clear all bunnies (convenience, not in upstream)
 *       B          — quit
 *     There is no pointer on device; bunnies spawn at the scene center and
 *     fan out with random velocities, mirroring the upstream point-spawn.
 *   - The scene is rendered into an 800x450 render texture (the upstream
 *     design resolution) which is then scaled letterboxed to the real
 *     surface, so the benchmark measures batching at the standard bunnymark
 *     resolution and fills a 1920x1080 (Ally) display.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle is the only exit
 *     path.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <stdbool.h>
#include <stdlib.h>  /* RL_MALLOC/RL_FREE */

#define TAG                 "bunnymark"
#define MAX_BUNNIES         80000
#define SPAWN_PER_FRAME     100
#define DESIGN_WIDTH        800
#define DESIGN_HEIGHT       450

/* Informational only: the upstream example reports batched draw calls using
 * rlgl's internal MAX_BATCH_ELEMENTS (8192). raylib's batching handles the
 * real limit internally; this value is kept for display parity. */
#define MAX_BATCH_ELEMENTS  8192

typedef struct Bunny {
    Vector2 position;
    Vector2 speed;
    Color   color;
} Bunny;

static const char *
lifecycle_name(PlayOSLifecycleEvent ev)
{
    switch (ev) {
    case PLAYOS_LIFECYCLE_FOREGROUND: return "foreground";
    case PLAYOS_LIFECYCLE_BACKGROUND: return "background";
    case PLAYOS_LIFECYCLE_SUSPEND:    return "suspend";
    case PLAYOS_LIFECYCLE_RESUME:     return "resume";
    case PLAYOS_LIFECYCLE_TERMINATE:  return "terminate";
    }
    return "unknown";
}

int
main(void)
{
    PLAYOS_LOG_I(TAG, "bunnymark starting");

    InitWindow(1280, 800, "PlayOS Bunnymark");
    /* No SetTargetFPS(): the benchmark runs uncapped (vsync-limited) so the
     * FPS readout reflects actual renderer throughput, as upstream does. */

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    /* Letterbox fit of the 800x450 design onto the real surface. */
    const float scale = (screenWidth / (float)DESIGN_WIDTH) <
                        (screenHeight / (float)DESIGN_HEIGHT)
                            ? (screenWidth / (float)DESIGN_WIDTH)
                            : (screenHeight / (float)DESIGN_HEIGHT);
    const float dstW = DESIGN_WIDTH * scale;
    const float dstH = DESIGN_HEIGHT * scale;
    const float dstX = (screenWidth - dstW) * 0.5f;
    const float dstY = (screenHeight - dstH) * 0.5f;

    RenderTexture canvas = LoadRenderTexture(DESIGN_WIDTH, DESIGN_HEIGHT);

    Texture2D texBunny = LoadTexture("resources/raybunny.png");

    Bunny *bunnies = (Bunny *)RL_MALLOC(MAX_BUNNIES * sizeof(Bunny));
    int bunniesCount = 0;

    bool paused = false;

    bool running   = true;
    bool suspended = false;

    while (running) {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended) {
            if (playos_lifecycle_wait(&ev, -1) != 1)
                continue;
            PLAYOS_LOG_I(TAG, "lifecycle: %s", lifecycle_name(ev));
            if (ev == PLAYOS_LIFECYCLE_TERMINATE)
                running = false;
            else if (ev == PLAYOS_LIFECYCLE_FOREGROUND || ev == PLAYOS_LIFECYCLE_RESUME)
                suspended = false;
            continue;
        }

        while (playos_lifecycle_poll(&ev) == 1) {
            PLAYOS_LOG_I(TAG, "lifecycle: %s", lifecycle_name(ev));
            if (ev == PLAYOS_LIFECYCLE_TERMINATE)
                running = false;
            else if (ev == PLAYOS_LIFECYCLE_BACKGROUND || ev == PLAYOS_LIFECYCLE_SUSPEND)
                suspended = true;
        }
        if (!running) break;
        if (suspended) continue;

        /* ── Input ── */
        const int  gamepad = 0;
        const bool gp      = IsGamepadAvailable(gamepad);

        /* A (hold): spawn bunnies at the scene center (upstream mouse hold). */
        if (gp && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            for (int i = 0; i < SPAWN_PER_FRAME; i++) {
                if (bunniesCount < MAX_BUNNIES) {
                    bunnies[bunniesCount].position =
                        (Vector2){ DESIGN_WIDTH / 2.0f, DESIGN_HEIGHT / 2.0f };
                    bunnies[bunniesCount].speed.x = (float)GetRandomValue(-250, 250);
                    bunnies[bunniesCount].speed.y = (float)GetRandomValue(-250, 250);
                    bunnies[bunniesCount].color = (Color){
                        GetRandomValue(50, 240),
                        GetRandomValue(80, 240),
                        GetRandomValue(100, 240), 255 };
                    bunniesCount++;
                }
            }
        }

        /* X: toggle pause (upstream P). */
        if (gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
            paused = !paused;

        /* LB: clear all bunnies (convenience, not in upstream). */
        if (gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1))
            bunniesCount = 0;

        /* ── Update ── */
        if (!paused) {
            for (int i = 0; i < bunniesCount; i++) {
                bunnies[i].position.x += bunnies[i].speed.x * GetFrameTime();
                bunnies[i].position.y += bunnies[i].speed.y * GetFrameTime();

                if (((bunnies[i].position.x + (float)texBunny.width / 2) > DESIGN_WIDTH) ||
                    ((bunnies[i].position.x + (float)texBunny.width / 2) < 0))
                    bunnies[i].speed.x *= -1;
                if (((bunnies[i].position.y + (float)texBunny.height / 2) > DESIGN_HEIGHT) ||
                    ((bunnies[i].position.y + (float)texBunny.height / 2 - 40) < 0))
                    bunnies[i].speed.y *= -1;
            }
        }

        /* ── Draw (into the 800x450 canvas) ── */
        BeginTextureMode(canvas);
        ClearBackground(RAYWHITE);

        for (int i = 0; i < bunniesCount; i++) {
            DrawTexture(texBunny, (int)bunnies[i].position.x,
                        (int)bunnies[i].position.y, bunnies[i].color);
        }

        DrawRectangle(0, 0, DESIGN_WIDTH, 40, BLACK);
        DrawText(TextFormat("bunnies: %i", bunniesCount), 120, 10, 20, GREEN);
        DrawText(TextFormat("batched draw calls: %i",
                            1 + bunniesCount / MAX_BATCH_ELEMENTS),
                 320, 10, 20, MAROON);
        if (paused)
            DrawText("PAUSED", 600, 10, 20, RED);
        DrawText("A = SPAWN   X = PAUSE   LB = CLEAR",
                 10, DESIGN_HEIGHT - 20, 10, BLACK);
        DrawFPS(10, 10);

        EndTextureMode();

        /* ── Present (scale the canvas to the real surface, letterboxed) ── */
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
                       (Rectangle){ 0, 0, DESIGN_WIDTH, (float)-DESIGN_HEIGHT },
                       (Rectangle){ dstX, dstY, dstW, dstH },
                       (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }

    /* ── De-initialization ── */
    RL_FREE(bunnies);
    UnloadTexture(texBunny);
    UnloadRenderTexture(canvas);
    CloseWindow();

    PLAYOS_LOG_I(TAG, "bunnymark exiting");
    return 0;
}
