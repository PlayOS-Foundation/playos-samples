/*
 * com.playos.sample-triangle — minimal PlayOS sample
 *
 * Reports system and display information through the public Platform API,
 * then exits cleanly. Hardware-accelerated triangle rendering is wired in a
 * later sprint; this binary proves the launch + display-query path.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

int main(void)
{
    PLAYOS_LOG_I("triangle", "sample-triangle starting");
    PLAYOS_LOG_I("triangle", "api_version=%u",
                 (unsigned)playos_system_api_version());

    PlayOSDisplayInfo info;
    if (playos_display_get_info(&info) == 0) {
        PLAYOS_LOG_I("triangle", "display=%dx%d@%.1fHz scale=%.2f",
                     info.width, info.height, info.refresh_rate, info.scale);
    } else {
        PLAYOS_LOG_W("triangle", "display info unavailable");
    }

    /* Non-blocking-ish wait so the sample behaves like a launched game. */
    PlayOSLifecycleEvent ev;
    (void)playos_lifecycle_wait(&ev, 200);

    PLAYOS_LOG_I("triangle", "sample-triangle exiting");
    return 0;
}
