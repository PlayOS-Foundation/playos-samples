/*
 * com.playos.sample-fog — PlayOS fog rendering
 *
 * A PlayOS port of raylib's `shaders_fog_rendering` example. A torus, a cube
 * and a sphere (plus a line of tori fading into the distance) are lit by a
 * single point light and wrapped in exponential distance fog, all computed
 * per-pixel in a GLSL ES 3.00 fragment shader.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - The shaders are GLSL ES 3.00 (`#version 300 es`, `in`/`out` qualifiers,
 *     `texture()`), matching the PlayOS raylib backend (GRAPHICS_API_OPENGL_ES3).
 *     The upstream example ships desktop GLSL 330 / mobile GLSL 100 pairs.
 *   - The unused `MaterialProperty` struct from the upstream fragment shader
 *     is dropped: ES 3.00 forbids sampler members in non-uniform structs and
 *     it is dead code.
 *   - Fog density is driven by the D-pad (up/down) on device; the upstream
 *     example uses KEY_UP/KEY_DOWN. Keyboard is kept as a desktop fallback.
 *   - The camera keeps upstream's CAMERA_ORBITAL mode, which continuously
 *     auto-orbits around the target — it needs no mouse on device.
 *   - Text is scaled to the actual surface instead of the fixed 800x450
 *     layout, so it stays readable on a 1920x1080 (Ally) display.
 *   - Lifecycle drives the loop: TERMINATE exits; BACKGROUND/SUSPEND idles
 *     the process, FOREGROUND/RESUME resumes it. The PlayOS raylib backend
 *     never feeds WindowShouldClose(), so the lifecycle is the only exit
 *     path.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#define TAG "fog"

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

int
main(void)
{
    PLAYOS_LOG_I(TAG, "fog starting");

    SetTraceLogCallback(raylib_trace_log);

    InitWindow(1280, 800, "PlayOS Fog");
    HideCursor();
    SetTargetFPS(60);

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float scale      = screenHeight / 450.0f;

    /* Define the camera to look into our 3d world. CAMERA_ORBITAL makes
     * UpdateCamera() continuously auto-orbit around the target, so no mouse
     * or stick input is required. */
    Camera camera = { 0 };
    camera.position   = (Vector3){ 2.0f, 2.0f, 6.0f };
    camera.target     = (Vector3){ 0.0f, 0.5f, 0.0f };
    camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    /* Load models and texture. */
    Model modelA = LoadModelFromMesh(GenMeshTorus(0.4f, 1.0f, 16, 32));
    Model modelB = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Model modelC = LoadModelFromMesh(GenMeshSphere(0.5f, 32, 32));
    Texture texture = LoadTexture("resources/texel_checker.png");
    PLAYOS_LOG_I(TAG, "texel_checker texture ready: %s", IsTextureValid(texture) ? "yes" : "NO");

    /* Assign the texture to the default model materials. */
    modelA.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    modelB.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    modelC.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

    /* Load the ES 3.00 shader pair and resolve the model/view uniforms.
     * matModel/matNormal are auto-updated by raylib during DrawModel; viewPos
     * must be set manually each frame. */
    Shader shader = LoadShader("resources/lighting.vs", "resources/fog.fs");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_VECTOR_VIEW]  = GetShaderLocation(shader, "viewPos");

    /* Ambient light level. */
    Vector4 ambient = (Vector4){ 0.2f, 0.2f, 0.2f, 1.0f };
    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, &ambient, SHADER_UNIFORM_VEC4);

    /* Fog color and density. */
    Vector4 fogColor = ColorNormalize(GRAY);
    int fogColorLoc = GetShaderLocation(shader, "fogColor");
    SetShaderValue(shader, fogColorLoc, &fogColor, SHADER_UNIFORM_VEC4);

    float fogDensity = 0.15f;
    int fogDensityLoc = GetShaderLocation(shader, "fogDensity");
    SetShaderValue(shader, fogDensityLoc, &fogDensity, SHADER_UNIFORM_FLOAT);

    /* A link failure silently falls back to the default shader, whose id is
     * still > 0 — so IsShaderValid() alone can't detect it. The default
     * shader has none of our custom uniforms, so any of them resolving to -1
     * proves we fell back. Log it but keep drawing with the default shader
     * so the scene is never a blank/white screen. */
    bool shaderReady = IsShaderValid(shader)
                       && (ambientLoc >= 0)
                       && (fogColorLoc >= 0)
                       && (fogDensityLoc >= 0);
    PLAYOS_LOG_I(TAG, "fog shader ready: %s (ambient=%d fogColor=%d fogDensity=%d)",
                 shaderReady ? "yes" : "NO", ambientLoc, fogColorLoc, fogDensityLoc);

    /* All models share the same shader. */
    modelA.materials[0].shader = shader;
    modelB.materials[0].shader = shader;
    modelC.materials[0].shader = shader;

    /* One white point light above/behind the camera. */
    CreateLight(LIGHT_POINT, (Vector3){ 0, 2, 6 }, Vector3Zero(), WHITE, shader);

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

        /* ── Update ── */
        UpdateCamera(&camera, CAMERA_ORBITAL);   // Auto-orbit around target

        /* Fog density: D-pad up/down on device, KEY_UP/KEY_DOWN on desktop. */
        const bool fogUp   = IsKeyDown(KEY_UP)
                             || (connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP));
        const bool fogDown = IsKeyDown(KEY_DOWN)
                             || (connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN));

        if (fogUp)
        {
            fogDensity += 0.001f;
            if (fogDensity > 1.0f) fogDensity = 1.0f;
        }
        if (fogDown)
        {
            fogDensity -= 0.001f;
            if (fogDensity < 0.0f) fogDensity = 0.0f;
        }
        SetShaderValue(shader, fogDensityLoc, &fogDensity, SHADER_UNIFORM_FLOAT);

        /* Rotate the torus. */
        modelA.transform = MatrixMultiply(modelA.transform, MatrixRotateX(-0.025f));
        modelA.transform = MatrixMultiply(modelA.transform, MatrixRotateZ(0.012f));

        /* The fragment shader needs the eye position for fog distance. */
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

        /* ── Draw ── */
        BeginDrawing();

        ClearBackground(GRAY);

        BeginMode3D(camera);

            /* The three models. */
            DrawModel(modelA, Vector3Zero(), 1.0f, WHITE);
            DrawModel(modelB, (Vector3){ -2.6f, 0, 0 }, 1.0f, WHITE);
            DrawModel(modelC, (Vector3){ 2.6f, 0, 0 }, 1.0f, WHITE);

            /* A line of tori fading into the distance makes the fog obvious. */
            for (int i = -20; i < 20; i += 2)
                DrawModel(modelA, (Vector3){ (float)i, 0, 2 }, 1.0f, WHITE);

        EndMode3D();

        DrawText(connected
                     ? TextFormat("D-PAD UP/DOWN changes fog density [%.2f]", fogDensity)
                     : TextFormat("KEY_UP/KEY_DOWN changes fog density [%.2f]", fogDensity),
                 10, 10, (int)(20 * scale), RAYWHITE);

        EndDrawing();
    }

    UnloadModel(modelA);
    UnloadModel(modelB);
    UnloadModel(modelC);
    UnloadTexture(texture);
    UnloadShader(shader);

    CloseWindow();
    PLAYOS_LOG_I(TAG, "fog exiting");
    return 0;
}
