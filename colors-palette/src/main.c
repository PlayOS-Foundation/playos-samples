/*
 * com.playos.sample-colors-palette — PlayOS colors palette viewer
 *
 * A PlayOS port of raylib's `shapes_colors_palette` example. It renders the
 * 21 built-in raylib colors as a grid and lets the player inspect each one.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - Input is read through raylib's native gamepad API (the PlayOS raylib
 *     backend exposes a single logical controller fed from libplayos):
 *       D-pad — move the selection cursor across the 7x3 grid
 *       A     — toggle showing every color's name (upstream SPACE)
 *       B     — quit
 *     Mouse-hover is kept for desktop builds and keyboard arrows / SPACE
 *     mirror the gamepad; on device the backend never feeds keyboard or
 *     mouse, so they are inert.
 *   - The grid is scaled to the actual screen size instead of the fixed
 *     800x450 layout, so it fills a 1920x1080 (Ally) display.
 *   - The window is fullscreen on device; dimensions are taken from
 *     GetScreenWidth()/GetScreenHeight() after InitWindow().
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle (or the B button)
 *     is the only reliable exit.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <stdbool.h>

#define TAG              "colors-palette"
#define MAX_COLORS_COUNT 21

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

/* Move the selection cursor by (dx, dy) grid steps, clamped to the 7x3 grid. */
static void
move_cursor(int *selected, int dx, int dy)
{
    int col = *selected % 7;
    int row = *selected / 7;

    col += dx;
    row += dy;

    if (col < 0) col = 0;
    if (col > 6) col = 6;
    if (row < 0) row = 0;
    if (row > 2) row = 2;

    *selected = row * 7 + col;
}

int
main(void)
{
    PLAYOS_LOG_I(TAG, "colors palette starting");

    InitWindow(1280, 800, "PlayOS Colors Palette");
    SetTargetFPS(60);

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    Color colors[MAX_COLORS_COUNT] = {
        DARKGRAY, MAROON, ORANGE, DARKGREEN, DARKBLUE, DARKPURPLE, DARKBROWN,
        GRAY,     RED,    GOLD,   LIME,      BLUE,     VIOLET,     BROWN,
        LIGHTGRAY, PINK,  YELLOW, GREEN,     SKYBLUE,  PURPLE,     BEIGE
    };

    const char *colorNames[MAX_COLORS_COUNT] = {
        "DARKGRAY", "MAROON", "ORANGE", "DARKGREEN", "DARKBLUE", "DARKPURPLE",
        "DARKBROWN", "GRAY", "RED", "GOLD", "LIME", "BLUE", "VIOLET", "BROWN",
        "LIGHTGRAY", "PINK", "YELLOW", "GREEN", "SKYBLUE", "PURPLE", "BEIGE"
    };

    Rectangle colorsRecs[MAX_COLORS_COUNT] = { 0 };
    int       colorState[MAX_COLORS_COUNT] = { 0 };

    /* Responsive 7x3 grid: fill the screen below a title/help header. */
    const float margin = 20.0f;
    const float gap    = 10.0f;
    const float header = 100.0f;
    const float cellW  = (screenWidth  - 2.0f * margin - 6.0f * gap) / 7.0f;
    const float cellH  = (screenHeight - header - 2.0f * margin - 2.0f * gap) / 3.0f;

    for (int i = 0; i < MAX_COLORS_COUNT; i++) {
        colorsRecs[i].x      = margin + (float)(i % 7) * (cellW + gap);
        colorsRecs[i].y      = header + (float)(i / 7) * (cellH + gap);
        colorsRecs[i].width  = cellW;
        colorsRecs[i].height = cellH;
    }

    int  selected  = 0;     /* cursor cell (0..20) */
    bool show_all  = false; /* A / SPACE toggles showing every name */
    bool running   = true;
    bool suspended = false;

    Vector2 mousePoint = { 0.0f, 0.0f };

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
        const int  gamepad   = 0;
        const bool connected = IsGamepadAvailable(gamepad);

        /* B quits. */
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        /* D-pad moves the cursor. */
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  move_cursor(&selected, -1,  0);
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) move_cursor(&selected,  1,  0);
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP))    move_cursor(&selected,  0, -1);
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  move_cursor(&selected,  0,  1);

        /* A toggles showing all names. */
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            show_all = !show_all;

        /* Desktop fallback: arrows + SPACE + mouse hover. */
        if (IsKeyPressed(KEY_LEFT))  move_cursor(&selected, -1,  0);
        if (IsKeyPressed(KEY_RIGHT)) move_cursor(&selected,  1,  0);
        if (IsKeyPressed(KEY_UP))    move_cursor(&selected,  0, -1);
        if (IsKeyPressed(KEY_DOWN))  move_cursor(&selected,  0,  1);
        if (IsKeyPressed(KEY_SPACE)) show_all = !show_all;

        mousePoint = GetMousePosition();
        for (int i = 0; i < MAX_COLORS_COUNT; i++)
            colorState[i] = CheckCollisionPointRec(mousePoint, colorsRecs[i]) ? 1 : 0;
        colorState[selected] = 1; /* cursor cell always highlighted */

        /* ── Draw ── */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("PlayOS Colors Palette", (int)margin, (int)(margin + 4), 32, BLACK);
        DrawText("D-PAD = SELECT   A = SHOW NAMES   B = QUIT",
                 (int)margin, (int)(header - 46), 18, GRAY);

        for (int i = 0; i < MAX_COLORS_COUNT; i++) {
            DrawRectangleRec(colorsRecs[i], Fade(colors[i], colorState[i] ? 0.6f : 1.0f));

            if (show_all || colorState[i]) {
                DrawRectangle((int)colorsRecs[i].x,
                              (int)(colorsRecs[i].y + colorsRecs[i].height - 26),
                              (int)colorsRecs[i].width, 20, BLACK);
                DrawRectangleLinesEx(colorsRecs[i], 6, Fade(BLACK, 0.3f));
                DrawText(colorNames[i],
                         (int)(colorsRecs[i].x + colorsRecs[i].width -
                               MeasureText(colorNames[i], 10) - 12),
                         (int)(colorsRecs[i].y + colorsRecs[i].height - 20),
                         10, colors[i]);
            }
        }

        /* Cursor cell gets a solid outline. */
        DrawRectangleLinesEx(colorsRecs[selected], 6, (Color){ 0, 150, 235, 255 });

        /* Selected color read-out with a swatch. */
        DrawRectangle((int)margin, screenHeight - 30, 22, 22, colors[selected]);
        DrawRectangleLines((int)margin, screenHeight - 30, 22, 22, BLACK);
        DrawText(colorNames[selected], (int)margin + 34, screenHeight - 26, 18, BLACK);

        EndDrawing();
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "colors palette exiting");
    return 0;
}
