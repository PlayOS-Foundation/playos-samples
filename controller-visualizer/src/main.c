/*
 * com.playos.sample-controller-visualizer — Raylib controller visualizer
 *
 * A PlayOS port of raylib's `core_input_gamepad` example. It renders an
 * on-screen gamepad and lights each control as the player actuates it.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - No gamepad enumeration/name detection, vibration, keyboard, or mouse:
 *     the PlayOS raylib backend provides rendering only. All input flows
 *     through libplayos' hardware-agnostic logical controller API.
 *   - No texture assets: the pad is drawn programmatically (this sample
 *     ships with no resource loader so it stays self-contained).
 *   - Triggers are read as axes in [0,1] (rest = 0) rather than raylib's
 *     [-1,1] (rest = -1), so the trigger bars fill from the bottom.
 *
 * The PlayOS raylib backend never feeds WindowShouldClose(), so the main
 * loop exits through playos_lifecycle_poll() (TERMINATE). The B button is
 * also wired as a convenient in-game quit.
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
draw_dpad(int cx, int cy, const PlayOSControllerState *st)
{
    const int arm   = 30;  /* arm length from center */
    const int thick = 22;  /* arm thickness */

    const bool up    = playos_input_button_down(st, PLAYOS_BUTTON_DPAD_UP);
    const bool down  = playos_input_button_down(st, PLAYOS_BUTTON_DPAD_DOWN);
    const bool left  = playos_input_button_down(st, PLAYOS_BUTTON_DPAD_LEFT);
    const bool right = playos_input_button_down(st, PLAYOS_BUTTON_DPAD_RIGHT);

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
draw_face_buttons(int cx, int cy, const PlayOSControllerState *st)
{
    const int r = 24;
    const int g = 70; /* gap from diamond center */

    struct {
        int x, y;
        playos_button_mask_t bit;
        const char *label;
    } buttons[4] = {
        { cx,         cy + g, PLAYOS_BUTTON_SOUTH, "A" },
        { cx + g,     cy,     PLAYOS_BUTTON_EAST,  "B" },
        { cx - g,     cy,     PLAYOS_BUTTON_WEST,  "X" },
        { cx,         cy - g, PLAYOS_BUTTON_NORTH, "Y" },
    };

    for (int i = 0; i < 4; i++) {
        const bool down = playos_input_button_down(st, buttons[i].bit);
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

        /* ── Controller state ── */
        PlayOSControllerState st;
        const bool connected = playos_input_get_controller_state(&st) == 0;

        /* B quits. */
        if (connected && playos_input_button_down(&st, PLAYOS_BUTTON_EAST)) {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        const float lx = apply_deadzone(st.axes[PLAYOS_AXIS_LEFT_X]);
        const float ly = apply_deadzone(st.axes[PLAYOS_AXIS_LEFT_Y]);
        const float rx = apply_deadzone(st.axes[PLAYOS_AXIS_RIGHT_X]);
        const float ry = apply_deadzone(st.axes[PLAYOS_AXIS_RIGHT_Y]);

        /* ── Render ── */
        BeginDrawing();
        ClearBackground((Color){ 14, 16, 24, 255 });

        DrawText("PlayOS Controller Visualizer", 40, 28, 30, kLabel);
        DrawText(connected ? "Controller connected — B quits"
                           : "No controller connected — B quits",
                 44, 70, 18, connected ? kDimLabel : (Color){ 236, 94, 106, 255 });

        /* Pad body. */
        DrawRectangleRounded((Rectangle){ 240, 130, 800, 520 }, 0.25f, 16, kBody);
        DrawRectangleRoundedLines((Rectangle){ 240, 130, 800, 520 }, 0.25f, 16, kWellRim);

        /* Left cluster: stick + D-pad. */
        draw_stick(430, 420, lx, ly,
                   connected && playos_input_button_down(&st, PLAYOS_BUTTON_L3));
        draw_dpad(430, 240, &st);

        /* Right cluster: stick + face buttons. */
        draw_stick(850, 420, rx, ry,
                   connected && playos_input_button_down(&st, PLAYOS_BUTTON_R3));
        draw_face_buttons(850, 240, &st);

        /* Bumpers. */
        draw_pill(300, 150, 150, 34, "L1",
                  connected && playos_input_button_down(&st, PLAYOS_BUTTON_L1));
        draw_pill(830, 150, 150, 34, "R1",
                  connected && playos_input_button_down(&st, PLAYOS_BUTTON_R1));

        /* Triggers. */
        draw_trigger(255, 196, st.axes[PLAYOS_AXIS_LEFT_TRIGGER], "L2");
        draw_trigger(999, 196, st.axes[PLAYOS_AXIS_RIGHT_TRIGGER], "R2");

        /* Center: Start / Select. */
        draw_pill(590, 480, 110, 34, "SELECT",
                  connected && playos_input_button_down(&st, PLAYOS_BUTTON_SELECT));
        draw_pill(760, 480, 110, 34, "START",
                  connected && playos_input_button_down(&st, PLAYOS_BUTTON_START));

        /* Raw readout. */
        DrawText(
            TextFormat("axes  L=(%+.2f, %+.2f)  R=(%+.2f, %+.2f)  L2=%.2f  R2=%.2f",
                       lx, ly, rx, ry,
                       st.axes[PLAYOS_AXIS_LEFT_TRIGGER],
                       st.axes[PLAYOS_AXIS_RIGHT_TRIGGER]),
            40, 740, 18, kDimLabel);

        EndDrawing();
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "controller-visualizer exiting");
    return 0;
}
