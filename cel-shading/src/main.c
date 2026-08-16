/*
 * com.playos.sample-cel-shading — PlayOS cel (toon) shading
 *
 * A PlayOS port of raylib's `shaders_cel_shading` example. A GLB car model is
 * rendered with a per-pixel toon shader (diffuse lighting quantized into a
 * user-adjustable number of bands) plus an inverted-hull outline pass that
 * draws extruded back faces as a silhouette.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - The shaders are GLSL ES 3.00 (`#version 300 es`, `in`/`out` qualifiers,
 *     `texture()`), matching the PlayOS raylib backend (GRAPHICS_API_OPENGL_ES3).
 *     The upstream example ships desktop GLSL 330 / mobile GLSL 100 pairs.
 *   - The cel and outline shaders are applied to *every* material on the
 *     model (not just materials[0]), so multi-material GLBs stay consistent.
 *   - Controls are mapped to the gamepad (see Controls in README.md); the
 *     upstream keyboard keys are kept as a desktop fallback.
 *   - The camera keeps upstream's CAMERA_ORBITAL mode, which continuously
 *     auto-orbits around the target — it needs no mouse on device.
 *   - Text is scaled to the actual surface instead of the fixed 800x450
 *     layout, so it stays readable on a 1920x1080 (Ally) display.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle (or the B button)
 *     is the only reliable exit.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"   // rlSetCullFace / RL_CULL_FACE_* for the outline pass

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#define TAG "cel-shading"

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

/* Forward raylib's trace log (including GL shader compile/link errors) into
 * the game's own stderr stream, which playos-init persists to
 * /data/log/game-<id>-stderr.log. */
static void
raylib_trace_log(int logLevel, const char *text, va_list args)
{
    (void)logLevel;
    char buf[512];
    vsnprintf(buf, sizeof(buf), text, args);
    playos_log(PLAYOS_LOG_INFO, "raylib", "%s", buf);
}

/* Apply a shader to every material on the model (upstream only touches
 * materials[0], which breaks multi-material GLBs). */
static void
set_all_material_shaders(Model *model, Shader shader)
{
    for (int m = 0; m < model->meshCount; m++)
        model->materials[m].shader = shader;
}

int
main(void)
{
    PLAYOS_LOG_I(TAG, "cel shading starting");

    SetTraceLogCallback(raylib_trace_log);

    InitWindow(1280, 800, "PlayOS Cel Shading");
    HideCursor();
    SetTargetFPS(60);

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float scale      = screenHeight / 450.0f;

    /* CAMERA_ORBITAL continuously auto-orbits around the target, so no mouse
     * or stick input is required. */
    Camera camera = { 0 };
    camera.position   = (Vector3){ 9.0f, 6.0f, 9.0f };
    camera.target     = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    /* Load the GLB car model (embedded textures). */
    Model model = LoadModel("resources/old_car_new.glb");
    PLAYOS_LOG_I(TAG, "model loaded: meshes=%d materials=%d (%s)",
                 model.meshCount, model.materialCount,
                 model.meshCount > 0 ? "ok" : "EMPTY");
    if (model.meshCount <= 0)
    {
        PLAYOS_LOG_E(TAG, "failed to load old_car_new.glb — exiting");
        CloseWindow();
        return 1;
    }

    /* Cel shader: quantized diffuse toon lighting. */
    Shader celShader = LoadShader("resources/cel.vs", "resources/cel.fs");
    celShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(celShader, "viewPos");

    /* Inverted-hull outline shader. */
    Shader outlineShader = LoadShader("resources/outline_hull.vs", "resources/outline_hull.fs");
    int outlineThicknessLoc = GetShaderLocation(outlineShader, "outlineThickness");

    /* numBands: toon quantization steps (2 = hard binary, 20 = near-smooth). */
    float numBands = 10.0f;
    int numBandsLoc = GetShaderLocation(celShader, "numBands");
    SetShaderValue(celShader, numBandsLoc, &numBands, SHADER_UNIFORM_FLOAT);

    /* Outline extrusion distance along the surface normal. */
    float outlineThickness = 0.005f;
    SetShaderValue(outlineShader, outlineThicknessLoc, &outlineThickness, SHADER_UNIFORM_FLOAT);

    /* Keep a copy of the default shader so toggling cel on/off can restore it. */
    Shader defaultShader = model.materials[0].shader;

    /* A link failure silently falls back to the default shader, whose id is
     * still > 0 — so IsShaderValid() alone can't detect it. The default
     * shader has none of our custom uniforms, so any of them resolving to -1
     * proves we fell back. */
    bool celReady = IsShaderValid(celShader)
                    && (celShader.locs[SHADER_LOC_VECTOR_VIEW] >= 0)
                    && (numBandsLoc >= 0);
    bool outlineReady = IsShaderValid(outlineShader)
                        && (outlineThicknessLoc >= 0);
    PLAYOS_LOG_I(TAG, "cel shader ready: %s (viewPos=%d numBands=%d)",
                 celReady ? "yes" : "NO",
                 celShader.locs[SHADER_LOC_VECTOR_VIEW], numBandsLoc);
    PLAYOS_LOG_I(TAG, "outline shader ready: %s (outlineThickness=%d)",
                 outlineReady ? "yes" : "NO", outlineThicknessLoc);

    /* One directional white light, angled so the toon bands are visible on
     * the model sides. Spins opposite to CAMERA_ORBITAL so lighting changes
     * as you watch. */
    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_DIRECTIONAL, (Vector3){ 50.0f, 50.0f, 50.0f }, Vector3Zero(), WHITE, celShader);

    bool celEnabled     = celReady;   /* start with cel applied if it linked */
    bool outlineEnabled = outlineReady;

    if (celEnabled)
        set_all_material_shaders(&model, celShader);

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

        /* A (or Z) toggles cel shading on/off. */
        if ((connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            || IsKeyPressed(KEY_Z))
        {
            celEnabled = celReady && !celEnabled;
            if (celEnabled) set_all_material_shaders(&model, celShader);
            else            set_all_material_shaders(&model, defaultShader);
            PLAYOS_LOG_I(TAG, "cel shading: %s", celEnabled ? "ON" : "OFF");
        }

        /* X (or C) toggles the outline pass on/off. */
        if ((connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
            || IsKeyPressed(KEY_C))
        {
            outlineEnabled = outlineReady && !outlineEnabled;
            PLAYOS_LOG_I(TAG, "outline: %s", outlineEnabled ? "ON" : "OFF");
        }

        /* D-pad up/down (or E/Q) steps the toon band count. */
        bool bandsUp   = IsKeyPressedRepeat(KEY_E)
                         || (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP));
        bool bandsDown = IsKeyPressedRepeat(KEY_Q)
                         || (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN));

        if (bandsUp)   numBands = Clamp(numBands + 1.0f, 2.0f, 20.0f);
        if (bandsDown) numBands = Clamp(numBands - 1.0f, 2.0f, 20.0f);
        SetShaderValue(celShader, numBandsLoc, &numBands, SHADER_UNIFORM_FLOAT);

        /* ── Update ── */
        UpdateCamera(&camera, CAMERA_ORBITAL);   // Auto-orbit around target

        /* The fragment shader needs the eye position for its (future) specular
         * / fresnel terms and point-light direction. */
        SetShaderValue(celShader, celShader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

        /* Spin the directional light opposite to CAMERA_ORBITAL, angled 45
         * degrees off vertical. */
        float t = (float)GetTime();
        lights[0].position = (Vector3){ sinf(-t * 0.3f) * 5.0f, 5.0f, cosf(-t * 0.3f) * 5.0f };

        for (int i = 0; i < MAX_LIGHTS; i++) UpdateLightValues(celShader, lights[i]);

        /* ── Draw ── */
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

            if (outlineEnabled)
            {
                /* Outline pass: cull front faces, draw extruded back faces as
                 * a silhouette. */
                set_all_material_shaders(&model, outlineShader);

                rlSetCullFace(RL_CULL_FACE_FRONT);
                DrawModel(model, Vector3Zero(), 0.75f, WHITE);
                rlSetCullFace(RL_CULL_FACE_BACK);

                if (celEnabled) set_all_material_shaders(&model, celShader);
                else            set_all_material_shaders(&model, defaultShader);
            }

            DrawModel(model, Vector3Zero(), 0.75f, WHITE);
            DrawSphereEx(lights[0].position, 0.2f, 50, 50, YELLOW);  // Light position indicator
            DrawGrid(10, 10.0f);

        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("Cel: %s  [A/Z]", celEnabled ? "ON" : "OFF"), 10, (int)(65 * scale), (int)(20 * scale), celEnabled ? DARKGREEN : DARKGRAY);
        DrawText(TextFormat("Outline: %s  [X/C]", outlineEnabled ? "ON" : "OFF"), 10, (int)(90 * scale), (int)(20 * scale), outlineEnabled ? DARKGREEN : DARKGRAY);
        DrawText(TextFormat("Bands: %.0f  [D-PAD/E/Q]", numBands), 10, (int)(115 * scale), (int)(20 * scale), DARKGRAY);
        DrawText("B = QUIT", 10, screenHeight - (int)(30 * scale), (int)(18 * scale), GRAY);

        EndDrawing();
    }

    UnloadModel(model);
    UnloadShader(celShader);
    UnloadShader(outlineShader);

    CloseWindow();
    PLAYOS_LOG_I(TAG, "cel shading exiting");
    return 0;
}
