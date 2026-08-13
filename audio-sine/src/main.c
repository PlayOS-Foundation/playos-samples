/*
 * com.playos.sample-audio — PlayOS audio sample placeholder
 *
 * Reports current audio device state through the public Platform API.
 * Actual sine-tone playback is wired in Sprint 8 (Raylib audio backend).
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

int main(void)
{
    PLAYOS_LOG_I("audio", "sample-audio starting (placeholder)");

    PlayOSAudioInfo info;
    if (playos_audio_get_info(&info) == 0) {
        PLAYOS_LOG_I("audio", "device=%dHz %dch %d-bit volume=%.2f muted=%d",
                     info.sample_rate, info.channels, info.bits_per_sample,
                     info.master_volume, info.muted);
    } else {
        PLAYOS_LOG_W("audio", "audio info unavailable");
    }

    PLAYOS_LOG_I("audio", "sine playback arrives in Sprint 8");

    PlayOSLifecycleEvent ev;
    (void)playos_lifecycle_wait(&ev, 200);

    PLAYOS_LOG_I("audio", "sample-audio exiting");
    return 0;
}
