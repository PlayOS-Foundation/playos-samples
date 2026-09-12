/*
 * com.playos.sample-controller-visualizer — Raylib controller visualizer
 *
 * A PlayOS port of raylib's `core_input_gamepad` example. It renders an
 * on-screen gamepad and lights each control as the player actuates it.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - Input is read through raylib's native gamepad API
 *     (IsGamepadAvailable / IsGamepadButtonDown / GetGamepadAxisMovement),
 *     which the PlayOS raylib backend feeds from libplayos' logical
 *     controller API. No gamepad enumeration, vibration, keyboard, or
 *     mouse: the backend exposes a single logical controller.
 *   - No texture assets: the pad is drawn programmatically (this sample
 *     ships with no resource loader so it stays self-contained).
 *   - Trigger axes are raylib's [-1,1] (rest = -1); the sample remaps them
 *     to [0,1] (rest = 0) so the trigger bars fill from the bottom.
 *
 * The PlayOS raylib backend never feeds WindowShouldClose(), so the main
 * loop exits through playos_lifecycle_poll() (TERMINATE): quitting is
 * owned by the platform pause overlay's Quit Game, not by a game button.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <stdbool.h>

#define TAG "controller-visualizer"

#define STICK_DEADZONE 0.10f

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

/* Zero an analog stick axis inside the deadzone. */
static float
apply_deadzone(float v)
{
    return (v > -STICK_DEADZONE && v < STICK_DEADZONE) ? 0.0f : v;
}

static const Color kBody      = {  24,  28,  40, 255 };
static const Color kWell      = {  18,  20,  30, 255 };
static const Color kWellRim   = {  70,  76, 100, 255 };
static const Color kControl   = {  54,  60,  82, 255 };
static const Color kControlHi = { 236,  94, 106, 255 }; /* pressed highlight */
static const Color kLabel     = { 230, 232, 240, 255 };
static const Color kDimLabel  = { 140, 146, 168, 255 };

/* Draw one analog stick: a fixed well plus a knob displaced by (x, y). */
static void
draw_stick(int cx, int cy, float x, float y, bool pressed)
{
    const int well_r = 72;
    const int knob_r = 34;

    DrawCircle(cx, cy, well_r, kWell);
    DrawCircleLines(cx, cy, well_r, kWellRim);
    DrawCircleLines(cx, cy, well_r - 1, kWellRim);

    const int dx = (int)(x * (float)(well_r - knob_r));
    const int dy = (int)(y * (float)(well_r - knob_r));

    DrawCircle(cx + dx, cy + dy, knob_r, pressed ? kControlHi : kControl);
    DrawCircleLines(cx + dx, cy + dy, knob_r, pressed ? kControlHi : kWellRim);
}

/* Draw the D-pad cross. */
static void
draw_dpad(int cx, int cy, int gamepad)
{
    const int arm   = 30;  /* arm length from center */
    const int thick = 22;  /* arm thickness */

    const bool up    = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP);
    const bool down  = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    const bool left  = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    const bool right = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

    const Color upc    = up    ? kControlHi : kControl;
    const Color downc  = down  ? kControlHi : kControl;
    const Color leftc  = left  ? kControlHi : kControl;
    const Color rightc = right ? kControlHi : kControl;

    DrawRectangle(cx - thick / 2, cy - arm, thick, 2 * arm, kWell);
    DrawRectangle(cx - arm, cy - thick / 2, 2 * arm, thick, kWell);
    DrawRectangleLines(cx - thick / 2, cy - arm, thick, 2 * arm, kWellRim);
    DrawRectangleLines(cx - arm, cy - thick / 2, 2 * arm, thick, kWellRim);

    /* Individual direction pads (drawn over the cross, with gaps). */
    DrawRectangle(cx - thick / 2, cy - arm, thick, arm - 4, upc);
    DrawRectangle(cx - thick / 2, cy + 4, thick, arm - 4, downc);
    DrawRectangle(cx - arm, cy - thick / 2, arm - 4, thick, leftc);
    DrawRectangle(cx + 4, cy - thick / 2, arm - 4, thick, rightc);
}

/* Draw the A/B/X/Y face-button diamond (Xbox layout). */
static void
draw_face_buttons(int cx, int cy, int gamepad)
{
    const int r = 24;
    const int g = 70; /* gap from diamond center */

    struct {
        int x, y;
        int button;
        const char *label;
    } buttons[4] = {
        { cx,         cy + g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  "A" },
        { cx + g,     cy,     GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, "B" },
        { cx - g,     cy,     GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  "X" },
        { cx,         cy - g, GAMEPAD_BUTTON_RIGHT_FACE_UP,    "Y" },
    };

    for (int i = 0; i < 4; i++) {
        const bool down = IsGamepadButtonDown(gamepad, buttons[i].button);
        const Color fill = down ? kControlHi : kControl;

        DrawCircle(buttons[i].x, buttons[i].y, r, fill);
        DrawCircleLines(buttons[i].x, buttons[i].y, r, down ? kControlHi : kWellRim);

        const int tw = MeasureText(buttons[i].label, 20);
        DrawText(buttons[i].label, buttons[i].x - tw / 2, buttons[i].y - 10, 20, kLabel);
    }
}

/* Draw a rounded label button. */
static void
draw_pill(int x, int y, int w, int h, const char *label, bool pressed)
{
    const Rectangle rec = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRounded(rec, 0.5f, 8, pressed ? kControlHi : kControl);
    DrawRectangleRoundedLines(rec, 0.5f, 8, pressed ? kControlHi : kWellRim);

    const int tw = MeasureText(label, 16);
    DrawText(label, x + (w - tw) / 2, y + (h - 16) / 2, 16, kLabel);
}

/* Draw a trigger as a vertical bar filled bottom-up by the axis [0,1]. */
static void
draw_trigger(int x, int y, float value, const char *label)
{
    const int w = 26;
    const int h = 88;

    DrawRectangle(x, y, w, h, kWell);
    DrawRectangleLines(x, y, w, h, kWellRim);

    const int fill = (int)(value * (float)(h - 6));
    if (fill > 0)
        DrawRectangle(x + 3, y + h - 3 - fill, w - 6, fill, kControlHi);

    const int tw = MeasureText(label, 14);
    DrawText(label, x + (w - tw) / 2, y - 18, 14, kDimLabel);
}

int main(void)
{
    PLAYOS_LOG_I(TAG, "controller-visualizer starting");

    InitWindow(1280, 800, "PlayOS Controller Visualizer");

    /* The PlayOS raylib backend forces fullscreen at the compositor's real
     * surface size (1920x1080), so the 1280x800 passed to InitWindow is only
     * a hint. Derive the real surface and center the pad on it — the upstream
     * sample hardcodes 1280x800 coordinates, which leaves the pad off-center. */
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const int pad_w = 800;
    const int pad_h = 520;
    const int pad_x = (sw - pad_w) / 2;
    const int pad_y = (sh - pad_h) / 2;

    bool running   = true;
    bool suspended = false;

    while (running) {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended) {
            /* Hidden or suspended: block until the next lifecycle event so
             * the process idles at near-zero CPU instead of busy-spinning. */
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

        /* ── Controller state (raylib native gamepad API) ── */
        const int gamepad = 0;   /* PlayOS exposes a single logical controller */
        const bool connected = IsGamepadAvailable(gamepad);

        const float lx = apply_deadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X));
        const float ly = apply_deadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y));
        const float rx = apply_deadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X));
        const float ry = apply_deadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y));

        /* Raylib triggers are [-1,1] (rest = -1); remap to [0,1] (rest = 0). */
        const float l2 = (GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER) + 1.0f) * 0.5f;
        const float r2 = (GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER) + 1.0f) * 0.5f;

        /* ── Render ── */
        BeginDrawing();
        ClearBackground((Color){ 14, 16, 24, 255 });

        DrawText("PlayOS Controller Visualizer", 40, 28, 30, kLabel);
        DrawText(connected ? TextFormat("%s connected", GetGamepadName(gamepad))
                           : "No controller connected",
                 44, 70, 18, connected ? kDimLabel : (Color){ 236, 94, 106, 255 });

        /* Pad body — centered on the compositor surface. */
        DrawRectangleRounded((Rectangle){ (float)pad_x, (float)pad_y, (float)pad_w, (float)pad_h },
                             0.25f, 16, kBody);
        DrawRectangleRoundedLines((Rectangle){ (float)pad_x, (float)pad_y, (float)pad_w, (float)pad_h },
                                  0.25f, 16, kWellRim);

        /* Left cluster: stick + D-pad. */
        draw_stick(pad_x + 190, pad_y + 380, lx, ly,
                   connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_THUMB));
        draw_dpad(pad_x + 190, pad_y + 190, gamepad);

        /* Right cluster: stick + face buttons. */
        draw_stick(pad_x + 610, pad_y + 380, rx, ry,
                   connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_THUMB));
        draw_face_buttons(pad_x + 610, pad_y + 190, gamepad);

        /* Bumpers. */
        draw_pill(pad_x + 60,  pad_y + 16, 150, 34, "L1",
                  connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
        draw_pill(pad_x + 590, pad_y + 16, 150, 34, "R1",
                  connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1));

        /* Triggers. */
        draw_trigger(pad_x + 15,  pad_y + 56, l2, "L2");
        draw_trigger(pad_x + 759, pad_y + 56, r2, "R2");

        /* Center: Start / Select (symmetric about the pad's vertical axis). */
        draw_pill(pad_x + 280, pad_y + 462, 110, 34, "SELECT",
                  connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT));
        draw_pill(pad_x + 410, pad_y + 462, 110, 34, "START",
                  connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT));

        /* Raw readout — centered below the pad. */
        {
            const char *readout = TextFormat(
                "axes  L=(%+.2f, %+.2f)  R=(%+.2f, %+.2f)  L2=%.2f  R2=%.2f",
                lx, ly, rx, ry, l2, r2);
            DrawText(readout, (sw - MeasureText(readout, 18)) / 2,
                     pad_y + pad_h + 36, 18, kDimLabel);
        }

        EndDrawing();
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "controller-visualizer exiting");
    return 0;
}
