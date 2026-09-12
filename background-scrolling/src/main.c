/*
 * com.playos.sample-background-scrolling — PlayOS parallax background scroller
 *
 * A PlayOS port of raylib's `textures_background_scrolling` example. Three
 * cyberpunk street layers scroll left at different speeds to produce a
 * parallax depth effect.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - The scene fills the full display: the upstream example is a fixed
 *     800x450 viewport with each layer scaled 2x. Here every layer is scaled
 *     so its height fills the surface (all three art strips are 192 px tall)
 *     and tiled horizontally enough times to cover the viewport, so the
 *     parallax fills a 1920x1080 (Ally) display without any per-layer
 *     cropping.
 *   - Scroll speeds are frame-rate independent (pixels/second) instead of the
 *     upstream fixed per-frame deltas, so the parallax rate is consistent on
 *     a 60 Hz display.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle is the only exit
 *     path.
 *   - Quitting is the platform's job: the pause overlay's Quit Game sends
 *     PLAYOS_LIFECYCLE_TERMINATE. The game does not exit on a button.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <stdbool.h>

#define TAG "background-scrolling"

/* Per-layer scroll speeds in pixels/second at the current scale. */
#define SPEED_BACK 24.0f
#define SPEED_MID  60.0f
#define SPEED_FORE 120.0f

/* Draw one parallax layer, tiling it horizontally to cover the viewport.
 * `scroll` advances from 0 toward -w and is reset by the caller; the extra
 * copy guarantees the right edge stays covered during the wrap. */
static void
draw_layer(Texture2D tex, float scroll, float scale, int screenWidth)
{
    if (tex.id == 0)
        return;

    const float w = (float)tex.width * scale;
    const int copies = (screenWidth + (int)w - 1) / (int)w + 1;

    for (int i = 0; i < copies; i++) {
        const float x = scroll + (float)i * w;
        DrawTextureEx(tex, (Vector2){ x, 0 }, 0.0f, scale, WHITE);
    }
}

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
    PLAYOS_LOG_I(TAG, "background scrolling starting");

    InitWindow(1280, 800, "PlayOS Background Scrolling");
    SetTargetFPS(60);

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    Texture2D background = LoadTexture("resources/cyberpunk_street_background.png");
    Texture2D midground  = LoadTexture("resources/cyberpunk_street_midground.png");
    Texture2D foreground = LoadTexture("resources/cyberpunk_street_foreground.png");

    if (background.id == 0 || midground.id == 0 || foreground.id == 0)
        PLAYOS_LOG_E(TAG, "failed to load one or more scrolling textures "
                          "(layers with id==0 are skipped)");

    float scrollingBack = 0.0f;
    float scrollingMid  = 0.0f;
    float scrollingFore = 0.0f;

    /* Scale so each layer's height fills the screen; all three art strips
     * are 192 px tall, so a single scale factor applies to every layer. */
    const float scale = (background.height > 0)
                            ? (float)screenHeight / (float)background.height
                            : 1.0f;

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

        /* ── Update ── */
        const float dt = GetFrameTime();

        scrollingBack -= SPEED_BACK * dt;
        scrollingMid  -= SPEED_MID  * dt;
        scrollingFore -= SPEED_FORE * dt;

        /* Wrap each layer once it has scrolled a full width off-screen left. */
        if (scrollingBack <= -(background.width * scale)) scrollingBack = 0.0f;
        if (scrollingMid  <= -(midground.width  * scale)) scrollingMid  = 0.0f;
        if (scrollingFore <= -(foreground.width * scale)) scrollingFore = 0.0f;

        /* ── Draw ── */
        BeginDrawing();
        ClearBackground(GetColor(0x052c46ff));

        draw_layer(background, scrollingBack, scale, screenWidth);
        draw_layer(midground,  scrollingMid,  scale, screenWidth);
        draw_layer(foreground, scrollingFore, scale, screenWidth);

        DrawText("BACKGROUND SCROLLING & PARALLAX", 10, 10, 20, RED);
        DrawText("(c) Cyberpunk Street Environment by Luis Zuno (@ansimuz)",
                 screenWidth - 330, screenHeight - 20, 10, RAYWHITE);

        EndDrawing();
    }

    UnloadTexture(background);
    UnloadTexture(midground);
    UnloadTexture(foreground);
    CloseWindow();

    PLAYOS_LOG_I(TAG, "background scrolling exiting");
    return 0;
}
