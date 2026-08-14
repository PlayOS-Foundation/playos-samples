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
    int rc = playos_lifecycle_wait(&ev, 200);
    if (rc == 1) {
        const char *name = "UNKNOWN";
        switch (ev) {
        case PLAYOS_LIFECYCLE_FOREGROUND: name = "FOREGROUND"; break;
        case PLAYOS_LIFECYCLE_BACKGROUND: name = "BACKGROUND"; break;
        case PLAYOS_LIFECYCLE_SUSPEND:    name = "SUSPEND";    break;
        case PLAYOS_LIFECYCLE_RESUME:     name = "RESUME";     break;
        case PLAYOS_LIFECYCLE_TERMINATE:  name = "TERMINATE";  break;
        }
        PLAYOS_LOG_I("input", "lifecycle event: %s", name);
    } else if (rc == 0) {
        PLAYOS_LOG_I("input", "no lifecycle event within 200ms");
    } else {
        PLAYOS_LOG_W("input", "lifecycle wait failed");
    }

    PLAYOS_LOG_I("input", "sample-input exiting");
    return 0;
}
