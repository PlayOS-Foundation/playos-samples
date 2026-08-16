/*
 * com.playos.sample-spotlight — PlayOS spotlight rendering
 *
 * A PlayOS port of raylib's `shaders_spotlight_rendering` example. A fragment
 * shader punches alpha "holes" into a full-screen black overlay so the scene
 * looks like a top-down stealth game: the left half of the screen is pitch
 * black except where spotlights fall, and the right half is dimly lit.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - The fragment shader is GLSL ES 1.00 (`#version 100`, `gl_FragColor`,
 *     `precision mediump float;`) to match the PlayOS raylib backend, which
 *     is built with GRAPHICS_API_OPENGL_ES2 (so its default vertex shader is
 *     `#version 100`). A `#version 300 es` fragment shader cannot link against
 *     that vertex shader ("all shaders must use same shading language
 *     version"), which silently falls back to the default shader.
 *   - Spot 0 (the "mouse" spotlight) is driven by the left stick (or D-pad)
 *     on device; the upstream example follows the mouse. Mouse is kept as a
 *     desktop fallback.
 *   - The scene is scaled to the actual surface instead of the fixed 800x450
 *     layout, so it fills a 1920x1080 (Ally) display: spot radii, star
 *     speeds, bob sizes and text all scale with screen height.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle (or the B button)
 *     is the only reliable exit.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define TAG       "spotlight"
#define MAX_SPOTS 3        // NOTE: must match the define in the shader
#define MAX_STARS 400

/* Spot data */
typedef struct Spot {
    Vector2 position;
    Vector2 speed;
    float   inner;
    float   radius;

    /* Shader uniform locations */
    int positionLoc;
    int innerLoc;
    int radiusLoc;
} Spot;

/* Stars in the star field have a position and velocity */
typedef struct Star {
    Vector2 position;
    Vector2 speed;
} Star;

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

/* Forward raylib's trace log (including GL shader compile errors) into the
 * game's own stderr stream, which playos-init persists to
 * /data/log/game-<id>-stderr.log. */
static void
raylib_trace_log(int logLevel, const char *text, va_list args)
{
    (void)logLevel;
    char buf[512];
    vsnprintf(buf, sizeof(buf), text, args);
    playos_log(PLAYOS_LOG_INFO, "raylib", "%s", buf);
}

static void
ResetStar(Star *star, float scale, int screenWidth, int screenHeight)
{
    star->position = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };

    star->speed.x = (float)GetRandomValue(-1000, 1000) / 100.0f * scale;
    star->speed.y = (float)GetRandomValue(-1000, 1000) / 100.0f * scale;

    while (!(fabsf(star->speed.x) + fabsf(star->speed.y) > 1.0f))
    {
        star->speed.x = (float)GetRandomValue(-1000, 1000) / 100.0f * scale;
        star->speed.y = (float)GetRandomValue(-1000, 1000) / 100.0f * scale;
    }

    star->position.x += star->speed.x * 8.0f;
    star->position.y += star->speed.y * 8.0f;
}

static void
UpdateStar(Star *star, int screenWidth, int screenHeight)
{
    star->position.x += star->speed.x;
    star->position.y += star->speed.y;

    if ((star->position.x < 0.0f) || (star->position.x > (float)screenWidth) ||
        (star->position.y < 0.0f) || (star->position.y > (float)screenHeight))
        ResetStar(star, 1.0f, screenWidth, screenHeight);
}

int
main(void)
{
    PLAYOS_LOG_I(TAG, "spotlight starting");

    SetTraceLogCallback(raylib_trace_log);

    InitWindow(1280, 800, "PlayOS Spotlight");
    HideCursor();
    SetTargetFPS(60);

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    /* Everything in the 800x450 upstream layout scales with screen height. */
    const float scale   = screenHeight / 450.0f;
    const float margin  = 64.0f * scale;
    const float bobSize = 64.0f * scale;
    const int   starSize = (int)(2.0f * scale) < 2 ? 2 : (int)(2.0f * scale);

    Texture texRay = LoadTexture("resources/raysan.png");
    PLAYOS_LOG_I(TAG, "raysan texture ready: %s", IsTextureValid(texRay) ? "yes" : "NO");

    Star stars[MAX_STARS] = { 0 };
    for (int n = 0; n < MAX_STARS; n++)
        ResetStar(&stars[n], scale, screenWidth, screenHeight);

    /* Progress all the stars on, so they don't all start in the centre. */
    for (int m = 0; m < screenWidth / 2; m++)
        for (int n = 0; n < MAX_STARS; n++)
            UpdateStar(&stars[n], screenWidth, screenHeight);

    int frameCounter = 0;

    /* Use the default vertex shader with our custom fragment shader. */
    Shader shdrSpot = LoadShader(0, "resources/spotlight.fs");
    if (!IsShaderValid(shdrSpot))
        PLAYOS_LOG_E(TAG, "spotlight.fs failed to compile/link — overlay disabled");

    /* Get the locations of the spots in the shader. */
    Spot spots[MAX_SPOTS];
    for (int i = 0; i < MAX_SPOTS; i++)
    {
        char posName[32]    = "spots[0].pos\0";
        char innerName[32]  = "spots[0].inner\0";
        char radiusName[32] = "spots[0].radius\0";

        posName[6]    = '0' + i;
        innerName[6]  = '0' + i;
        radiusName[6] = '0' + i;

        spots[i].positionLoc = GetShaderLocation(shdrSpot, posName);
        spots[i].innerLoc    = GetShaderLocation(shdrSpot, innerName);
        spots[i].radiusLoc   = GetShaderLocation(shdrSpot, radiusName);

        PLAYOS_LOG_I(TAG, "spot %d uniforms: pos=%d inner=%d radius=%d",
                     i, spots[i].positionLoc, spots[i].innerLoc, spots[i].radiusLoc);
    }

    /* Tell the shader how wide the screen is so we can have a pitch black
     * half and a dimly lit half. */
    float sw = (float)GetScreenWidth();
    int screenWidthLoc = GetShaderLocation(shdrSpot, "screenWidth");
    PLAYOS_LOG_I(TAG, "screenWidth uniform loc=%d", screenWidthLoc);

    /* A link failure silently falls back to the default shader, whose id is
     * still > 0 — so IsShaderValid() alone can't detect it. The default
     * shader has none of our custom uniforms, so any of them resolving to -1
     * proves we fell back. Gate the overlay on this so a failed shader can
     * never cover the whole scene in opaque white. */
    bool shaderReady = IsShaderValid(shdrSpot)
                       && (screenWidthLoc >= 0)
                       && (spots[0].positionLoc >= 0);
    PLAYOS_LOG_I(TAG, "spotlight shader ready: %s", shaderReady ? "yes" : "NO");
    SetShaderValue(shdrSpot, screenWidthLoc, &sw, SHADER_UNIFORM_FLOAT);

    /* Randomize the locations and velocities of the spotlights and
     * initialize the shader locations. */
    for (int i = 0; i < MAX_SPOTS; i++)
    {
        spots[i].position.x = (float)GetRandomValue((int)margin, screenWidth - (int)margin);
        spots[i].position.y = (float)GetRandomValue((int)margin, screenHeight - (int)margin);
        spots[i].speed = (Vector2){ 0.0f, 0.0f };

        while ((fabsf(spots[i].speed.x) + fabsf(spots[i].speed.y)) < 2.0f)
        {
            spots[i].speed.x = GetRandomValue(-400, 40) / 25.0f * scale;
            spots[i].speed.y = GetRandomValue(-400, 40) / 25.0f * scale;
        }

        spots[i].inner  = 28.0f * (i + 1) * scale;
        spots[i].radius = 48.0f * (i + 1) * scale;

        SetShaderValue(shdrSpot, spots[i].positionLoc, &spots[i].position.x, SHADER_UNIFORM_VEC2);
        SetShaderValue(shdrSpot, spots[i].innerLoc, &spots[i].inner, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shdrSpot, spots[i].radiusLoc, &spots[i].radius, SHADER_UNIFORM_FLOAT);
    }

    bool running   = true;
    bool suspended = false;

    while (running)
    {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended)
        {
            if (playos_lifecycle_wait(&ev, -1) != 1)
                continue;
            PLAYOS_LOG_I(TAG, "lifecycle: %s", lifecycle_name(ev));
            if (ev == PLAYOS_LIFECYCLE_TERMINATE)
                running = false;
            else if (ev == PLAYOS_LIFECYCLE_FOREGROUND || ev == PLAYOS_LIFECYCLE_RESUME)
                suspended = false;
            continue;
        }

        while (playos_lifecycle_poll(&ev) == 1)
        {
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
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        frameCounter++;

        /* Move the stars, resetting them if they go offscreen. */
        for (int n = 0; n < MAX_STARS; n++)
            UpdateStar(&stars[n], screenWidth, screenHeight);

        /* Update the spots and send them to the shader. */
        for (int i = 0; i < MAX_SPOTS; i++)
        {
            if (i == 0)
            {
                /* Spot 0 is player-controlled: left stick (or D-pad) on
                 * device, mouse on desktop. */
                if (connected)
                {
                    const float stickSpeed = 1500.0f * scale;
                    float ax = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
                    float ay = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);

                    spots[i].position.x += ax * stickSpeed * GetFrameTime();
                    spots[i].position.y += ay * stickSpeed * GetFrameTime();

                    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
                        spots[i].position.x -= 800.0f * scale * GetFrameTime();
                    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
                        spots[i].position.x += 800.0f * scale * GetFrameTime();
                    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP))
                        spots[i].position.y -= 800.0f * scale * GetFrameTime();
                    if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
                        spots[i].position.y += 800.0f * scale * GetFrameTime();
                }
                else
                {
                    Vector2 mp = GetMousePosition();
                    spots[i].position.x = mp.x;
                    spots[i].position.y = screenHeight - mp.y;
                }

                if (spots[i].position.x < 0.0f) spots[i].position.x = 0.0f;
                if (spots[i].position.x > (float)screenWidth) spots[i].position.x = (float)screenWidth;
                if (spots[i].position.y < 0.0f) spots[i].position.y = 0.0f;
                if (spots[i].position.y > (float)screenHeight) spots[i].position.y = (float)screenHeight;
            }
            else
            {
                spots[i].position.x += spots[i].speed.x;
                spots[i].position.y += spots[i].speed.y;

                if (spots[i].position.x < margin) spots[i].speed.x = -spots[i].speed.x;
                if (spots[i].position.x > (screenWidth - margin)) spots[i].speed.x = -spots[i].speed.x;
                if (spots[i].position.y < margin) spots[i].speed.y = -spots[i].speed.y;
                if (spots[i].position.y > (screenHeight - margin)) spots[i].speed.y = -spots[i].speed.y;
            }

            SetShaderValue(shdrSpot, spots[i].positionLoc, &spots[i].position.x, SHADER_UNIFORM_VEC2);
        }

        /* ── Draw ── */
        BeginDrawing();

        ClearBackground(DARKBLUE);

        /* Draw stars. */
        for (int n = 0; n < MAX_STARS; n++)
            DrawRectangle((int)stars[n].position.x, (int)stars[n].position.y, starSize, starSize, WHITE);

        /* Draw the bobs (raysan heads) orbiting the centre. */
        for (int i = 0; i < 16; i++)
        {
            float x = (screenWidth  / 2.0f) + cosf((frameCounter + i * 8) / 51.45f) * (screenWidth  / 2.2f) - bobSize / 2.0f;
            float y = (screenHeight / 2.0f) + sinf((frameCounter + i * 8) / 17.87f) * (screenHeight / 4.2f) - bobSize / 2.0f;
            DrawTextureEx(texRay, (Vector2){ x, y }, 0.0f, bobSize / 64.0f, WHITE);
        }

        /* Draw the spotlights (the shader does the lighting). */
        if (shaderReady)
        {
            BeginShaderMode(shdrSpot);
                DrawRectangle(0, 0, screenWidth, screenHeight, WHITE);
            EndShaderMode();
        }

        DrawFPS(10, 10);

        DrawText(connected ? "Left stick / D-PAD moves the spotlight!" : "Move the mouse!",
                 10, (int)(30 * scale), (int)(20 * scale), GREEN);
        DrawText("Pitch Black", (int)(screenWidth * 0.2f), screenHeight / 2, (int)(20 * scale), GREEN);
        DrawText("Dark", (int)(screenWidth * 0.66f), screenHeight / 2, (int)(20 * scale), GREEN);
        DrawText("B = QUIT", 10, screenHeight - (int)(30 * scale), (int)(18 * scale), GRAY);

        EndDrawing();
    }

    UnloadTexture(texRay);
    UnloadShader(shdrSpot);

    CloseWindow();
    PLAYOS_LOG_I(TAG, "spotlight exiting");
    return 0;
}
