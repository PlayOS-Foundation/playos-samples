/*
 * com.playos.sample-audio — PlayOS audio sample (Sprint 8)
 *
 * Renders a reference card and continuously plays a 440 Hz stereo sine tone
 * through Raylib's miniaudio/ALSA backend. Lifecycle events drive the
 * pause/resume behavior: BACKGROUND/SUSPEND pauses the stream, FOREGROUND/
 * RESUME resumes it, and TERMINATE stops playback and exits.
 *
 * Under QEMU/CI there is no audio hardware: IsAudioDeviceReady() is false and
 * the card still renders, reporting "Audio device: unavailable".
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

#include "raylib.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TAG               "audio-sine"
#define SAMPLE_RATE       44100
#define CHANNELS          2
#define BITS_PER_SAMPLE   16
#define TONE_FREQ         440.0f
#define CHUNK_FRAMES      4410   /* 100 ms of audio per push */

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

/* Fill `frames` stereo frames (2 * frames int16 samples) with a continuous
 * 440 Hz sine. `phase` advances across calls so chunks join seamlessly. */
static void
fill_sine(int16_t *samples, int frames, double *phase)
{
    const double step = 2.0 * M_PI * TONE_FREQ / (double)SAMPLE_RATE;

    for (int i = 0; i < frames; i++) {
        const int16_t v = (int16_t)(32000.0 * sin(*phase));
        samples[i * 2]     = v;
        samples[i * 2 + 1] = v;
        *phase += step;
        if (*phase >= 2.0 * M_PI)
            *phase -= 2.0 * M_PI;
    }
}

/* Apply a single lifecycle event to the run/suspend state, pausing or
 * resuming the audio stream to match. */
static void
handle_lifecycle(PlayOSLifecycleEvent ev, bool *running, bool *suspended,
                 bool playing, AudioStream stream,
                 PlayOSLifecycleEvent *last_ev)
{
    *last_ev = ev;
    switch (ev) {
    case PLAYOS_LIFECYCLE_TERMINATE:
        PLAYOS_LOG_I(TAG, "lifecycle: terminate");
        *running = false;
        break;
    case PLAYOS_LIFECYCLE_BACKGROUND:
    case PLAYOS_LIFECYCLE_SUSPEND:
        *suspended = true;
        if (playing && IsAudioStreamPlaying(stream))
            PauseAudioStream(stream);
        break;
    case PLAYOS_LIFECYCLE_FOREGROUND:
    case PLAYOS_LIFECYCLE_RESUME:
        *suspended = false;
        if (playing && !IsAudioStreamPlaying(stream))
            ResumeAudioStream(stream);
        break;
    }
}

int main(void)
{
    PLAYOS_LOG_I(TAG, "sample-audio starting");

    InitWindow(1280, 720, "PlayOS Audio Sine");

    /* InitAudioDevice() must run before IsAudioDeviceReady() — the latter
     * only returns true once the device has actually been initialized. */
    InitAudioDevice();
    const bool audio_ready = IsAudioDeviceReady();
    if (!audio_ready)
        PLAYOS_LOG_W(TAG, "audio device unavailable — rendering headless");

    AudioStream stream = { 0 };
    if (audio_ready) {
        stream = LoadAudioStream(SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS);
        if (!IsAudioStreamValid(stream)) {
            PLAYOS_LOG_W(TAG, "failed to create audio stream");
        } else {
            SetAudioStreamVolume(stream, 1.0f);
        }
    }

    const bool stream_ok = audio_ready && IsAudioStreamValid(stream);

    int16_t *chunk = (int16_t *)MemAlloc((size_t)CHUNK_FRAMES * CHANNELS
                                         * sizeof(int16_t));
    double phase = 0.0;

    if (stream_ok && chunk) {
        fill_sine(chunk, CHUNK_FRAMES, &phase);
        UpdateAudioStream(stream, chunk, CHUNK_FRAMES);
        PlayAudioStream(stream);
    }

    bool running   = true;
    bool suspended = false;
    bool playing   = stream_ok;

    PlayOSLifecycleEvent last_ev = PLAYOS_LIFECYCLE_FOREGROUND;

    while (running) {
        /* ── Lifecycle (required: the only reliable exit path) ── */
        PlayOSLifecycleEvent ev;

        if (suspended) {
            /* Hidden or suspended: block until the next lifecycle event so
             * the process idles at near-zero CPU (the BACKGROUND contract)
             * instead of busy-spinning. */
            if (playos_lifecycle_wait(&ev, -1) != 1)
                continue;
            handle_lifecycle(ev, &running, &suspended, playing, stream, &last_ev);
            continue;
        }

        while (playos_lifecycle_poll(&ev) == 1) {
            handle_lifecycle(ev, &running, &suspended, playing, stream, &last_ev);
            if (!running)
                break;
        }
        if (!running)
            break;
        if (suspended)
            continue;   /* just backgrounded — block above */

        /* ── Keep the stream fed (ring-buffer refill). ── */
        if (playing && IsAudioStreamProcessed(stream)) {
            fill_sine(chunk, CHUNK_FRAMES, &phase);
            UpdateAudioStream(stream, chunk, CHUNK_FRAMES);
        }

        /* ── Render ── */
        PlayOSAudioInfo info;
        (void)playos_audio_get_info(&info);

        char line1[128];
        char line2[128];
        char line3[128];
        char line4[128];
        char line5[128];
        snprintf(line1, sizeof(line1), "Tone: %.0f Hz  stereo  %d-bit",
                 TONE_FREQ, BITS_PER_SAMPLE);
        snprintf(line2, sizeof(line2), "Device: %s",
                 audio_ready ? "available" : "unavailable");
        snprintf(line3, sizeof(line3), "Master volume: %.0f%%  %s",
                 info.master_volume * 100.0f,
                 info.muted ? "(muted)" : "");
        snprintf(line4, sizeof(line4), "State: %s%s",
                 lifecycle_name(last_ev),
                 suspended ? " (paused)" : "");
        snprintf(line5, sizeof(line5), "B = quit");

        BeginDrawing();
        ClearBackground((Color){ 12, 14, 22, 255 });

        DrawText("PlayOS Audio Sine", 40, 40, 34, RAYWHITE);
        DrawText(line1, 40, 96, 22, LIGHTGRAY);
        DrawText(line2, 40, 132, 22, audio_ready ? SKYBLUE : ORANGE);
        DrawText(line3, 40, 168, 22, LIGHTGRAY);
        DrawText(line4, 40, 204, 22, suspended ? YELLOW : LIGHTGRAY);
        DrawText(line5, 40, 256, 20, DARKGRAY);
        DrawFPS(1200, 40);

        EndDrawing();
    }

    if (chunk)
        MemFree(chunk);

    if (stream_ok) {
        StopAudioStream(stream);
        UnloadAudioStream(stream);
        CloseAudioDevice();
    }

    CloseWindow();
    PLAYOS_LOG_I(TAG, "sample-audio exiting");
    return 0;
}
