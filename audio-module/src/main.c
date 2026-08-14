/*
 * com.playos.sample-audio-module — PlayOS module (tracker music) player
 *
 * A PlayOS port of raylib's `audio_module_playing` example. It streams a
 * FastTracker II module (.xm) through raylib's jar_xm backend and renders a
 * pulsing-circle visualizer plus a playback progress bar.
 *
 * Differences from the upstream raylib example (all intentional):
 *   - Input is read through raylib's native gamepad API (the PlayOS raylib
 *     backend exposes a single logical controller fed from libplayos):
 *       A      — restart the module
 *       X      — pause / resume
 *       D-pad  — raise / lower playback pitch (speed)
 *       B      — quit
 *     The upstream keyboard bindings (SPACE / P / UP / DOWN) are also
 *     accepted for desktop builds; on device the backend never feeds
 *     keyboard state, so they are inert.
 *   - No FLAG_MSAA_4X_HINT: the PlayOS backend targets GLES2, where MSAA is
 *     not guaranteed.
 *   - The window is fullscreen on device; dimensions are taken from
 *     GetScreenWidth()/GetScreenHeight() after InitWindow().
 *   - Lifecycle drives pause/resume: BACKGROUND/SUSPEND pauses the stream
 *     and idles the process, FOREGROUND/RESUME resumes it, TERMINATE exits.
 *     The PlayOS raylib backend never feeds WindowShouldClose(), so the
 *     lifecycle (or the B button) is the only reliable exit.
 *   - Audio-unavailable is handled gracefully (headless render under
 *     QEMU/CI, matching audio-sine).
 *   - The module ships under resources/ and is resolved relative to the
 *     game's working directory (/data/games/<app-id>/), which playos-init
 *     seeds from /usr/share/playos/games/<app-id>/ on first boot.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>

#define TAG          "audio-module"
#define MAX_CIRCLES  64
#define PITCH_STEP   0.01f

typedef struct {
    Vector2 position;
    float radius;
    float alpha;
    float speed;
    Color color;
} CircleWave;

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

/* Apply a single lifecycle event: keep the music stream paused while hidden
 * and resume it on return to foreground, honouring any user pause. */
static void
handle_lifecycle(PlayOSLifecycleEvent ev, bool *running, bool *suspended,
                 bool music_valid, Music music, bool user_paused)
{
    switch (ev) {
    case PLAYOS_LIFECYCLE_TERMINATE:
        PLAYOS_LOG_I(TAG, "lifecycle: terminate");
        *running = false;
        break;
    case PLAYOS_LIFECYCLE_BACKGROUND:
    case PLAYOS_LIFECYCLE_SUSPEND:
        *suspended = true;
        if (music_valid && !user_paused)
            PauseMusicStream(music);
        break;
    case PLAYOS_LIFECYCLE_FOREGROUND:
    case PLAYOS_LIFECYCLE_RESUME:
        *suspended = false;
        if (music_valid && !user_paused)
            ResumeMusicStream(music);
        break;
    }
}

int main(void)
{
    PLAYOS_LOG_I(TAG, "module player starting");

    InitWindow(1280, 800, "PlayOS Module Player");

    /* InitAudioDevice() must run before IsAudioDeviceReady() — the latter
     * only returns true once the device has actually been initialized. */
    InitAudioDevice();
    const bool audio_ready = IsAudioDeviceReady();
    if (!audio_ready)
        PLAYOS_LOG_W(TAG, "audio device unavailable — rendering headless");

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    Color colors[14] = { ORANGE, RED, GOLD, LIME, BLUE, VIOLET, BROWN,
                         LIGHTGRAY, PINK, YELLOW, GREEN, SKYBLUE, PURPLE,
                         BEIGE };

    /* Creates some circles for visual effect. */
    CircleWave circles[MAX_CIRCLES] = { 0 };
    for (int i = MAX_CIRCLES - 1; i >= 0; i--) {
        circles[i].alpha = 0.0f;
        circles[i].radius = (float)GetRandomValue(10, 40);
        circles[i].position.x = (float)GetRandomValue((int)circles[i].radius,
                                                      (int)(screenWidth - circles[i].radius));
        circles[i].position.y = (float)GetRandomValue((int)circles[i].radius,
                                                      (int)(screenHeight - circles[i].radius));
        circles[i].speed = (float)GetRandomValue(1, 100) / 2000.0f;
        circles[i].color = colors[GetRandomValue(0, 13)];
    }

    Music music = { 0 };
    if (audio_ready) {
        music = LoadMusicStream("resources/mini1111.xm");
        if (!IsMusicValid(music))
            PLAYOS_LOG_W(TAG, "failed to load module resources/mini1111.xm");
    }
    const bool music_valid = audio_ready && IsMusicValid(music);

    music.looping = false;
    float pitch = 1.0f;

    if (music_valid)
        PlayMusicStream(music);

    float timePlayed = 0.0f;
    bool running     = true;
    bool suspended   = false;
    bool user_paused = false;

    PlayOSLifecycleEvent last_ev = PLAYOS_LIFECYCLE_FOREGROUND;

    while (running) {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended) {
            /* Hidden or suspended: block until the next lifecycle event so
             * the process idles at near-zero CPU instead of busy-spinning. */
            if (playos_lifecycle_wait(&ev, -1) != 1)
                continue;
            handle_lifecycle(ev, &running, &suspended, music_valid, music, user_paused);
            last_ev = ev;
            continue;
        }

        while (playos_lifecycle_poll(&ev) == 1) {
            handle_lifecycle(ev, &running, &suspended, music_valid, music, user_paused);
            last_ev = ev;
            if (!running)
                break;
        }
        if (!running)
            break;
        if (suspended)
            continue;   /* just backgrounded — block above */

        /* ── Update ── */
        if (music_valid)
            UpdateMusicStream(music);   /* refill music buffer */

        /* ── Controller (single logical gamepad) + keyboard ── */
        const int  gamepad   = 0;
        const bool connected = IsGamepadAvailable(gamepad);

        /* B quits. */
        if (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            PLAYOS_LOG_I(TAG, "B pressed — exiting");
            break;
        }

        /* A restarts the module from the top. */
        const bool restart =
            (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) ||
            IsKeyPressed(KEY_SPACE);
        if (restart && music_valid) {
            StopMusicStream(music);
            PlayMusicStream(music);
            user_paused = false;
        }

        /* X toggles pause / resume. */
        const bool pause_toggle =
            (connected && IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) ||
            IsKeyPressed(KEY_P);
        if (pause_toggle && music_valid) {
            user_paused = !user_paused;
            if (user_paused) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        /* D-pad up/down adjusts pitch (speed). */
        const bool up =
            (connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)) ||
            IsKeyDown(KEY_UP);
        const bool down =
            (connected && IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) ||
            IsKeyDown(KEY_DOWN);
        if (down) pitch -= PITCH_STEP;
        else if (up) pitch += PITCH_STEP;
        if (music_valid)
            SetMusicPitch(music, pitch);

        /* Time played, scaled to the progress-bar width. */
        if (music_valid) {
            const float len = GetMusicTimeLength(music);
            timePlayed = (len > 0.0f)
                ? GetMusicTimePlayed(music) / len * (float)(screenWidth - 40)
                : 0.0f;
        } else {
            timePlayed = 0.0f;
        }

        /* Circle animation pauses while the music is paused or suspended. */
        const bool anim_paused = user_paused || suspended;
        for (int i = MAX_CIRCLES - 1; (i >= 0) && !anim_paused; i--) {
            circles[i].alpha  += circles[i].speed;
            circles[i].radius += circles[i].speed * 10.0f;

            if (circles[i].alpha > 1.0f) circles[i].speed *= -1;

            if (circles[i].alpha <= 0.0f) {
                circles[i].alpha = 0.0f;
                circles[i].radius = (float)GetRandomValue(10, 40);
                circles[i].position.x = (float)GetRandomValue((int)circles[i].radius,
                                                              (int)(screenWidth - circles[i].radius));
                circles[i].position.y = (float)GetRandomValue((int)circles[i].radius,
                                                              (int)(screenHeight - circles[i].radius));
                circles[i].color = colors[GetRandomValue(0, 13)];
                circles[i].speed = (float)GetRandomValue(1, 100) / 2000.0f;
            }
        }

        /* ── Draw ── */
        BeginDrawing();
        ClearBackground((Color){ 12, 14, 22, 255 });

        for (int i = MAX_CIRCLES - 1; i >= 0; i--)
            DrawCircleV(circles[i].position, circles[i].radius,
                        Fade(circles[i].color, circles[i].alpha));

        /* Progress bar. */
        DrawRectangle(20, screenHeight - 32, screenWidth - 40, 12, LIGHTGRAY);
        DrawRectangle(20, screenHeight - 32, (int)timePlayed, 12, MAROON);
        DrawRectangleLines(20, screenHeight - 32, screenWidth - 40, 12, GRAY);

        /* Help / status panel. */
        DrawRectangle(20, 20, 460, 196, (Color){ 24, 28, 40, 220 });
        DrawRectangleLines(20, 20, 460, 196, (Color){ 70, 76, 100, 255 });
        DrawText("PlayOS Module Player", 40, 34, 22, RAYWHITE);
        DrawText("A = RESTART        X = PAUSE/RESUME", 40, 70, 18, LIGHTGRAY);
        DrawText("D-PAD UP/DOWN = PITCH (SPEED)", 40, 96, 18, LIGHTGRAY);
        DrawText("B = QUIT", 40, 122, 18, LIGHTGRAY);
        DrawText(TextFormat("PITCH: %.3f", pitch), 40, 148, 18, SKYBLUE);

        const char *status;
        if (!music_valid)           status = "audio unavailable";
        else if (suspended)         status = "suspended";
        else if (user_paused)       status = "paused";
        else                        status = "playing";
        DrawText(TextFormat("Status: %s  (%s)", status, lifecycle_name(last_ev)),
                 40, 174, 18,
                 !music_valid ? ORANGE : (user_paused || suspended ? YELLOW : LIGHTGRAY));

        DrawFPS(screenWidth - 100, 20);

        EndDrawing();
    }

    if (music_valid) {
        UnloadMusicStream(music);   /* unload music stream buffers from RAM */
        CloseAudioDevice();         /* music streaming is stopped automatically */
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "module player exiting");
    return 0;
}
