/*
 * sdk-reference — a minimal PlayOS game built entirely with the PlayOS SDK.
 *
 * One source for all three SDK profiles:
 *   device    musl cross toolchain, raylib PLATFORM_PLAYOS, real libplayos
 *   desktop   host gcc, raylib's desktop backend, the libplayos host shim
 *   emulator  the device build, run in the PlayOS QEMU image
 *
 * It exercises the public libplayos surface a real game needs — system
 * identity, lifecycle, controller input, per-game storage and structured
 * logging — while rendering an animated scene, so every profile can be
 * validated end to end. See README.md.
 *
 * SPDX-License-Identifier: MIT
 */
#include <raylib.h>
#include <playos/playos.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REFERENCE_VERSION "1.0.0"
#define SESSION_FILE      "sessions.txt"

typedef struct {
    int                   running;
    int                   backgrounded;
    long                  frames;
    unsigned              sessions;
    char                  last_event[32];
    char                  saves_path[512];
    int                   controller_connected;
    PlayOSControllerState controller;
} ReferenceState;

static ReferenceState g; /* one mutable state struct per process */

static const char *event_name(PlayOSLifecycleEvent e)
{
    switch (e) {
    case PLAYOS_LIFECYCLE_FOREGROUND: return "FOREGROUND";
    case PLAYOS_LIFECYCLE_BACKGROUND: return "BACKGROUND";
    case PLAYOS_LIFECYCLE_SUSPEND:    return "SUSPEND";
    case PLAYOS_LIFECYCLE_RESUME:     return "RESUME";
    case PLAYOS_LIFECYCLE_TERMINATE:  return "TERMINATE";
    }
    return "UNKNOWN";
}

/* How many times the reference has been run, persisted with the API's
 * atomic write so the lifetime story is exercised too. */
static void load_session_count(void)
{
    if (g.saves_path[0] == '\0')
        return;

    char path[600];
    snprintf(path, sizeof(path), "%s/%s", g.saves_path, SESSION_FILE);

    FILE *f = fopen(path, "r");
    if (!f)
        return;

    unsigned long n = 0;
    if (fscanf(f, "%lu", &n) == 1)
        g.sessions = (unsigned)n;
    fclose(f);
}

static void save_session_count(void)
{
    if (g.saves_path[0] == '\0')
        return;

    char path[600];
    char text[32];
    int len = snprintf(text, sizeof(text), "%u\n", g.sessions);
    snprintf(path, sizeof(path), "%s/%s", g.saves_path, SESSION_FILE);

    if (playos_storage_atomic_write(path, text, (size_t)len) != 0)
        PLAYOS_LOG_W("save", "could not write %s", path);
}

static void handle_lifecycle(void)
{
    PlayOSLifecycleEvent ev;

    while (playos_lifecycle_poll(&ev) == 1) {
        snprintf(g.last_event, sizeof(g.last_event), "%s", event_name(ev));
        PLAYOS_LOG_I("lifecycle", "event: %s", event_name(ev));

        switch (ev) {
        case PLAYOS_LIFECYCLE_FOREGROUND:
            g.backgrounded = 0;
            break;
        case PLAYOS_LIFECYCLE_BACKGROUND:
            /* Hidden behind the overlay or the shell: stop rendering. */
            g.backgrounded = 1;
            break;
        case PLAYOS_LIFECYCLE_SUSPEND:
            save_session_count();
            g.backgrounded = 1;
            break;
        case PLAYOS_LIFECYCLE_RESUME:
            g.backgrounded = 0;
            break;
        case PLAYOS_LIFECYCLE_TERMINATE:
            save_session_count();
            g.running = 0;
            break;
        }
    }
}

static void draw_hud(void)
{
    int y = 16;
    const int step = 22;

    DrawText(TextFormat("PlayOS SDK Reference %s", REFERENCE_VERSION), 16, y, 20, RAYWHITE);
    y += step + 6;
    DrawText(TextFormat("api_version %u   os %s", (unsigned)playos_system_api_version(),
                        playos_system_os_version()),
             16, y, 18, (Color){ 160, 200, 255, 255 });
    y += step;
    DrawText(TextFormat("device: %s", playos_system_device_model()), 16, y, 18, SKYBLUE);
    y += step;
    DrawText(TextFormat("gpu: %s", playos_system_gpu_description()), 16, y, 18, SKYBLUE);
    y += step;
    DrawText(TextFormat("saves: %s", g.saves_path[0] ? g.saves_path : "(unavailable)"),
             16, y, 18, GRAY);
    y += step;
    DrawText(TextFormat("runs %u   frames %ld   event %s",
                        g.sessions, g.frames, g.last_event),
             16, y, 18, LIGHTGRAY);
    y += step;
    DrawText(TextFormat("controller: %s%s", g.controller_connected ? "connected" : "none",
                        g.controller_connected
                            ? (playos_input_button_down(&g.controller, PLAYOS_BUTTON_SOUTH)
                                   ? "   [A]" : "")
                            : ""),
             16, y, 18, g.controller_connected ? GREEN : ORANGE);
    y += step + 6;
    DrawText("D-pad/sticks move the marker   B exits", 16, y, 16, DARKGRAY);
    DrawFPS(GetScreenWidth() - 90, 16);
}

int main(void)
{
    memset(&g, 0, sizeof(g));
    g.running = 1;
    snprintf(g.last_event, sizeof(g.last_event), "none");

    PLAYOS_LOG_I("boot", "sdk-reference %s starting (libplayos %u, os %s)",
                 REFERENCE_VERSION,
                 (unsigned)playos_system_api_version(),
                 playos_system_os_version());

    if (playos_system_api_version() != (uint32_t)PLAYOS_API_VERSION)
        PLAYOS_LOG_W("boot", "API mismatch: runtime %u, headers %u",
                     (unsigned)playos_system_api_version(),
                     (unsigned)PLAYOS_API_VERSION);

    const char *saves = playos_storage_get_saves_path();
    if (saves) {
        snprintf(g.saves_path, sizeof(g.saves_path), "%s", saves);
        load_session_count();
        PLAYOS_LOG_I("save", "saves at %s (%u previous run(s), %lld bytes free)",
                     g.saves_path, g.sessions,
                     (long long)playos_storage_free_bytes());
    } else {
        PLAYOS_LOG_W("save", "no saves path available");
    }
    g.sessions++;

    InitWindow(1280, 720, "PlayOS SDK Reference");
    SetTargetFPS(60);

    while (g.running && !WindowShouldClose()) {
        handle_lifecycle();

        if (g.backgrounded) {
            /* Lifecycle contract: near-zero CPU while hidden. The PlayOS
             * backend does not feed WindowShouldClose, so a terminate only
             * arrives through handle_lifecycle(). */
            WaitTime(0.05);
            continue;
        }

        g.controller_connected = playos_input_controller_connected();
        if (g.controller_connected)
            (void)playos_input_get_controller_state(&g.controller);

        /* This sample treats B as exit so a device-only tester can leave it;
         * a real game is free to use B however it likes (it is not reserved). */
        if (g.controller_connected &&
            playos_input_button_down(&g.controller, PLAYOS_BUTTON_EAST))
            g.running = 0;

        float t = (float)GetTime();
        float cx = (float)GetScreenWidth() * 0.5f;
        float cy = (float)GetScreenHeight() * 0.5f;

        if (g.controller_connected) {
            cx += g.controller.axes[PLAYOS_AXIS_LEFT_X] * 240.0f;
            cy += g.controller.axes[PLAYOS_AXIS_LEFT_Y] * 160.0f;
        }

        BeginDrawing();
        ClearBackground((Color){ 20, 41, 76, 255 });

        for (int i = 0; i < 7; i++) {
            float phase = t * 0.8f + (float)i * 0.6f;
            Rectangle r = { cx, cy, 90.0f + (float)i * 12.0f, 90.0f + (float)i * 12.0f };
            Vector2 origin = { r.width * 0.5f, r.height * 0.5f };
            Color c = { (unsigned char)(255 - i * 28),
                        (unsigned char)(120 + i * 18),
                        (unsigned char)(76 + i * 22), 200 };
            DrawRectanglePro(r, origin, phase * 57.2958f, c);
            DrawRectangleLinesEx(r, 1.0f, (Color){ 230, 235, 245, 90 });
        }

        DrawCircleV((Vector2){ cx, cy }, 8.0f, ORANGE);
        draw_hud();
        EndDrawing();

        g.frames++;
    }

    save_session_count();
    CloseWindow();
    PLAYOS_LOG_I("exit", "sdk-reference exiting after %ld frames", g.frames);
    return 0;
}
