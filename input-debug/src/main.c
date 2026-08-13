/*
 * com.playos.sample-input — PlayOS logical controller state sample
 *
 * Reports the hardware-agnostic logical controller snapshot, then exits.
 *
 * SPDX-License-Identifier: MIT
 */

#include <playos/playos.h>

int main(void)
{
    PLAYOS_LOG_I("input", "sample-input starting");

    if (!playos_input_controller_connected()) {
        PLAYOS_LOG_W("input", "no controller connected");
    } else {
        PlayOSControllerState state;
        if (playos_input_get_controller_state(&state) == 0) {
            PLAYOS_LOG_I("input", "buttons=0x%08x", (unsigned)state.buttons);
            PLAYOS_LOG_I("input",
                         "axes L=(%.2f,%.2f) R=(%.2f,%.2f) trig=(%.2f,%.2f)",
                         state.axes[PLAYOS_AXIS_LEFT_X],
                         state.axes[PLAYOS_AXIS_LEFT_Y],
                         state.axes[PLAYOS_AXIS_RIGHT_X],
                         state.axes[PLAYOS_AXIS_RIGHT_Y],
                         state.axes[PLAYOS_AXIS_LEFT_TRIGGER],
                         state.axes[PLAYOS_AXIS_RIGHT_TRIGGER]);
        } else {
            PLAYOS_LOG_W("input", "controller state unavailable");
        }
    }

    PlayOSLifecycleEvent ev;
    (void)playos_lifecycle_wait(&ev, 200);

    PLAYOS_LOG_I("input", "sample-input exiting");
    return 0;
}
