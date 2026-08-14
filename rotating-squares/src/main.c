/*
 * com.playos.sample-rotating-squares — Raylib rotating-squares sample
 *
 * Demonstrates the intended engine split for PlayOS:
 *   - Raylib renders (Wayland/EGL/GLES2 via the PLATFORM_PLAYOS backend)
 *   - libplayos supplies lifecycle + controller input
 *
 * The PlayOS raylib backend never feeds WindowShouldClose(), so the main
 * loop exits through playos_lifecycle_poll() (TERMINATE). The B button is
 * also wired as a convenient in-game quit.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <math.h>
#include <stdbool.h>

#define TAG "rotating-squares"

/* Apply a single lifecycle event to the run/suspend state. */
static void
handle_lifecycle(PlayOSLifecycleEvent ev, bool *running, bool *suspended)
{
    switch (ev) {
    case PLAYOS_LIFECYCLE_TERMINATE:
        PLAYOS_LOG_I(TAG, "lifecycle: terminate");
        *running = false;
        break;
    case PLAYOS_LIFECYCLE_BACKGROUND:
    case PLAYOS_LIFECYCLE_SUSPEND:
        *suspended = true;
        break;
    case PLAYOS_LIFECYCLE_FOREGROUND:
    case PLAYOS_LIFECYCLE_RESUME:
        *suspended = false;
        break;
    }
}

int main(void)
{
    PLAYOS_LOG_I(TAG, "rotating-squares starting");

    InitWindow(1280, 800, "PlayOS Rotating Squares");

    const int width  = GetScreenWidth();
    const int height = GetScreenHeight();

    PLAYOS_LOG_I(TAG, "render surface %dx%d", width, height);

    /* Grid geometry — sized from the real (compositor-assigned) surface. */
    const int cell = 96;                 /* spacing between square centers */
    const int half = 34;                 /* half side of each square */
    const int cols = width / cell + 2;   /* overscan a bit to fill edges */
    const int rows = height / cell + 2;
    const int ox = (width  - (cols - 1) * cell) / 2;
    const int oy = (height - (rows - 1) * cell) / 2;

    bool running   = true;
    bool suspended = false;

    while (running) {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended) {
            /* Hidden or suspended: block until the next lifecycle event so
             * the process idles at near-zero CPU (the BACKGROUND contract)
             * instead of busy-spinning. */
            if (playos_lifecycle_wait(&ev, -1) != 1)
                continue;
            handle_lifecycle(ev, &running, &suspended);
            continue;
        }

        while (playos_lifecycle_poll(&ev) == 1) {
            handle_lifecycle(ev, &running, &suspended);
            if (!running)
                break;
        }
        if (!running) break;
        if (suspended) continue;   /* just backgrounded — block above */

        /* ── Controller (B quits) ── */
        PlayOSControllerState ctrl;
        if (playos_input_get_controller_state(&ctrl) == 0 &&
            playos_input_button_down(&ctrl, PLAYOS_BUTTON_EAST)) {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        /* ── Render ── */
        const double t = GetTime();

        BeginDrawing();
        ClearBackground((Color){ 16, 18, 26, 255 });

        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                const float phase = (float)((r * 3 + c * 7) % 11);
                const float dir   = ((r + c) & 1) ? -1.0f : 1.0f;
                const float speed = 40.0f + 12.0f * phase;
                const float angle = fmodf(dir * (float)t * speed, 360.0f);

                const Rectangle rect = {
                    (float)(ox + c * cell - half),
                    (float)(oy + r * cell - half),
                    (float)(half * 2),
                    (float)(half * 2)
                };
                const Vector2 origin = { (float)half, (float)half };

                const float hue = fmodf(
                    ((float)(r * cols + c) / (float)(cols * rows)) * 360.0f
                        + (float)t * 24.0f,
                    360.0f);

                DrawRectanglePro(rect, origin, angle,
                                 ColorFromHSV(hue, 0.72f, 0.95f));
            }
        }

        EndDrawing();
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "rotating-squares exiting");
    return 0;
}
