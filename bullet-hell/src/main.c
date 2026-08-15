/*
 * com.playos.sample-bullet-hell — PlayOS bullet-hell visualizer
 *
 * A PlayOS port of raylib's `shapes_bullet_hell` example. A spinning "magic
 * circle" at the screen center continuously sprays a radial fan of bullets;
 * the player tunes the spawner parameters live to stress-test the renderer.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - Input is read through raylib's native gamepad API (the PlayOS raylib
 *     backend exposes a single logical controller fed from libplayos):
 *       D-pad Left/Right — change bullet row count (upstream A/D)
 *       D-pad Up/Down    — change bullet speed (upstream W/S)
 *       X / Y            — decrease / increase spawn cooldown (upstream Z/X)
 *       A (hold)         — advance the spawn angle increment (upstream SPACE)
 *       RB               — toggle draw method (upstream ENTER)
 *       LB               — clear all bullets (upstream C)
 *       B                — quit
 *     The upstream keyboard controls are retained as a desktop fallback; on
 *     device the backend never feeds keyboard or mouse, so they are inert.
 *   - The whole scene is rendered into an 800x450 render texture (the
 *     upstream design resolution) which is then scaled letterboxed to the
 *     real surface, so the fixed 16:9 layout fills a 1920x1080 (Ally) display
 *     without any per-object coordinate scaling.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle (or the B button)
 *     is the only reliable exit.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <math.h>    /* cosf(), sinf() */
#include <stdbool.h>
#include <stdlib.h>  /* calloc(), free() via RL_CALLOC/RL_FREE */

#define TAG          "bullet-hell"
#define MAX_BULLETS  500000
#define DESIGN_WIDTH  800
#define DESIGN_HEIGHT 450

typedef struct Bullet {
    Vector2 position;       /* Bullet position on screen */
    Vector2 acceleration;   /* Pixels to add to position every frame */
    bool    disabled;       /* Skip processing/draw when out of screen */
    Color   color;          /* Bullet color */
} Bullet;

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
    PLAYOS_LOG_I(TAG, "bullet hell starting");

    InitWindow(1280, 800, "PlayOS Bullet Hell");
    SetTargetFPS(60);

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

    /* Scene canvas at the design resolution; blitted scaled to the surface. */
    RenderTexture canvas = LoadRenderTexture(DESIGN_WIDTH, DESIGN_HEIGHT);

    /* Bullets definition. */
    Bullet *bullets = (Bullet *)RL_CALLOC(MAX_BULLETS, sizeof(Bullet));
    int bulletCount         = 0;
    int bulletDisabledCount = 0;
    int bulletRadius        = 10;
    float bulletSpeed       = 3.0f;
    int bulletRows          = 6;
    Color bulletColor[2]    = { RED, BLUE };

    /* Spawner variables. */
    float baseDirection      = 0;
    int angleIncrement       = 5;
    float spawnCooldown      = 2;
    float spawnCooldownTimer = spawnCooldown;

    /* Magic circle. */
    float magicCircleRotation = 0;

    /* Pre-render the bullet texture once for performance. */
    RenderTexture bulletTexture = LoadRenderTexture(24, 24);
    BeginTextureMode(bulletTexture);
        DrawCircle(12, 12, (float)bulletRadius, WHITE);
        DrawCircleLines(12, 12, (float)bulletRadius, BLACK);
    EndTextureMode();

    bool drawInPerformanceMode = true; /* switch between DrawTexture() and DrawCircle() */

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

        /* B quits. */
        if (gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        /* D-pad Left/Right: change bullet rows (keyboard A/D). */
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) ||
            IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
            if (bulletRows > 1) bulletRows--;
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) ||
            IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            if (bulletRows < 359) bulletRows++;

        /* D-pad Up/Down: change bullet speed (keyboard W/S). */
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)) ||
            IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            bulletSpeed += 0.25f;
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) ||
            IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            if (bulletSpeed > 0.50f) bulletSpeed -= 0.25f;

        /* X / Y: decrease / increase spawn cooldown (keyboard Z/X). */
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) ||
            IsKeyPressed(KEY_Z))
            if (spawnCooldown > 1) spawnCooldown--;
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP)) ||
            IsKeyPressed(KEY_X))
            spawnCooldown++;

        /* RB: toggle draw method (keyboard ENTER). */
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) ||
            IsKeyPressed(KEY_ENTER))
            drawInPerformanceMode = !drawInPerformanceMode;

        /* A (hold): advance spawn angle increment (keyboard SPACE). */
        if ((gp && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) ||
            IsKeyDown(KEY_SPACE)) {
            angleIncrement += 1;
            angleIncrement %= 360;
        }

        /* LB: clear bullets (keyboard C). */
        if ((gp && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) ||
            IsKeyPressed(KEY_C)) {
            bulletCount = 0;
            bulletDisabledCount = 0;
        }

        /* ── Update ── */
        /* Reset the bullet index when exhausted; new bullets reuse disabled slots. */
        if (bulletCount >= MAX_BULLETS) {
            bulletCount = 0;
            bulletDisabledCount = 0;
        }

        spawnCooldownTimer--;
        if (spawnCooldownTimer < 0) {
            spawnCooldownTimer = spawnCooldown;

            float degreesPerRow = 360.0f / bulletRows;
            for (int row = 0; row < bulletRows; row++) {
                if (bulletCount < MAX_BULLETS) {
                    bullets[bulletCount].position = (Vector2){ DESIGN_WIDTH / 2.0f, DESIGN_HEIGHT / 2.0f };
                    bullets[bulletCount].disabled = false;
                    bullets[bulletCount].color    = bulletColor[row % 2];

                    float bulletDirection = baseDirection + (degreesPerRow * row);

                    bullets[bulletCount].acceleration = (Vector2){
                        bulletSpeed * cosf(bulletDirection * DEG2RAD),
                        bulletSpeed * sinf(bulletDirection * DEG2RAD)
                    };

                    bulletCount++;
                }
            }

            baseDirection += angleIncrement;
        }

        /* Advance bullets; disable those that leave the design viewport. */
        for (int i = 0; i < bulletCount; i++) {
            if (!bullets[i].disabled) {
                bullets[i].position.x += bullets[i].acceleration.x;
                bullets[i].position.y += bullets[i].acceleration.y;

                if ((bullets[i].position.x < -bulletRadius * 2) ||
                    (bullets[i].position.x > DESIGN_WIDTH + bulletRadius * 2) ||
                    (bullets[i].position.y < -bulletRadius * 2) ||
                    (bullets[i].position.y > DESIGN_HEIGHT + bulletRadius * 2)) {
                    bullets[i].disabled = true;
                    bulletDisabledCount++;
                }
            }
        }

        /* ── Draw (into the 800x450 canvas) ── */
        BeginTextureMode(canvas);
        ClearBackground(RAYWHITE);

        /* Magic circle. */
        magicCircleRotation++;
        DrawRectanglePro((Rectangle){ DESIGN_WIDTH / 2.0f, DESIGN_HEIGHT / 2.0f, 120, 120 },
                         (Vector2){ 60.0f, 60.0f }, magicCircleRotation, PURPLE);
        DrawRectanglePro((Rectangle){ DESIGN_WIDTH / 2.0f, DESIGN_HEIGHT / 2.0f, 120, 120 },
                         (Vector2){ 60.0f, 60.0f }, magicCircleRotation + 45, PURPLE);
        DrawCircleLines(DESIGN_WIDTH / 2, DESIGN_HEIGHT / 2, 70, BLACK);
        DrawCircleLines(DESIGN_WIDTH / 2, DESIGN_HEIGHT / 2, 50, BLACK);
        DrawCircleLines(DESIGN_WIDTH / 2, DESIGN_HEIGHT / 2, 30, BLACK);

        /* Bullets. */
        if (drawInPerformanceMode) {
            for (int i = 0; i < bulletCount; i++) {
                if (!bullets[i].disabled) {
                    DrawTexture(bulletTexture.texture,
                                (int)(bullets[i].position.x - bulletTexture.texture.width * 0.5f),
                                (int)(bullets[i].position.y - bulletTexture.texture.height * 0.5f),
                                bullets[i].color);
                }
            }
        } else {
            for (int i = 0; i < bulletCount; i++) {
                if (!bullets[i].disabled) {
                    DrawCircleV(bullets[i].position, (float)bulletRadius, bullets[i].color);
                    DrawCircleLinesV(bullets[i].position, (float)bulletRadius, BLACK);
                }
            }
        }

        /* Controls panel. */
        DrawRectangle(10, 10, 280, 180, (Color){ 0, 0, 0, 200 });
        DrawText("Controls:", 20, 20, 10, LIGHTGRAY);
        DrawText("- D-Pad L/R: Bullet rows", 40, 40, 10, LIGHTGRAY);
        DrawText("- D-Pad U/D: Bullet speed", 40, 60, 10, LIGHTGRAY);
        DrawText("- X/Y: Spawn cooldown", 40, 80, 10, LIGHTGRAY);
        DrawText("- A (hold): Angle increment", 40, 100, 10, LIGHTGRAY);
        DrawText("- RB: Draw method", 40, 120, 10, LIGHTGRAY);
        DrawText("- LB: Clear bullets", 40, 140, 10, LIGHTGRAY);
        DrawText("- B: Quit", 40, 160, 10, LIGHTGRAY);

        /* Draw method indicator. */
        DrawRectangle(610, 10, 170, 30, (Color){ 0, 0, 0, 200 });
        if (drawInPerformanceMode)
            DrawText("Draw method: DrawTexture(*)", 620, 20, 10, GREEN);
        else
            DrawText("Draw method: DrawCircle(*)", 620, 20, 10, RED);

        /* Stats bar. */
        DrawRectangle(135, 410, 530, 30, (Color){ 0, 0, 0, 200 });
        DrawText(TextFormat("[ FPS: %d, Bullets: %d, Rows: %d, Speed: %.2f, Angle: %d, Cooldown: %.0f ]",
                            GetFPS(), bulletCount - bulletDisabledCount, bulletRows, bulletSpeed,
                            angleIncrement, spawnCooldown),
                 155, 420, 10, GREEN);

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
    UnloadRenderTexture(bulletTexture);
    UnloadRenderTexture(canvas);
    RL_FREE(bullets);
    CloseWindow();

    PLAYOS_LOG_I(TAG, "bullet hell exiting");
    return 0;
}
