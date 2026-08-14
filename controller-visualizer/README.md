# com.playos.sample-controller-visualizer

Raylib sample that renders an on-screen gamepad and lights each control as the
player actuates it — a PlayOS port of raylib's
[`core_input_gamepad`](https://github.com/raysan5/raylib/blob/master/examples/core/core_input_gamepad.c)
example.

This demonstrates the intended engine split on PlayOS: **Raylib owns
rendering**, while **libplayos owns lifecycle and input**. Because the PlayOS
raylib backend provides rendering only, this sample drops raylib's gamepad
enumeration, vibration, keyboard, and mouse handling — all input is read
through libplayos' hardware-agnostic logical controller API. The gamepad
itself is drawn programmatically (no texture assets), and triggers are read
as `[0,1]` axes (rest = 0) rather than raylib's `[-1,1]` convention.

The main loop exits through `playos_lifecycle_poll()` (TERMINATE); the B
button is also wired as an in-game quit.

## Button / axis mapping

| Control | PlayOS input |
|---|---|
| A / B / X / Y | `PLAYOS_BUTTON_SOUTH` / `EAST` / `WEST` / `NORTH` |
| D-pad | `PLAYOS_BUTTON_DPAD_*` |
| L1 / R1 | `PLAYOS_BUTTON_L1` / `R1` |
| L3 / R3 (stick click) | `PLAYOS_BUTTON_L3` / `R3` |
| Select / Start | `PLAYOS_BUTTON_SELECT` / `START` |
| Left stick | `PLAYOS_AXIS_LEFT_X`, `PLAYOS_AXIS_LEFT_Y` (deadzone 0.10) |
| Right stick | `PLAYOS_AXIS_RIGHT_X`, `PLAYOS_AXIS_RIGHT_Y` (deadzone 0.10) |
| L2 / R2 (analog) | `PLAYOS_AXIS_LEFT_TRIGGER`, `PLAYOS_AXIS_RIGHT_TRIGGER` (`[0,1]`) |

## API surface used

Raylib:

- `InitWindow`, `BeginDrawing` / `ClearBackground`, `EndDrawing`, `CloseWindow`
- `DrawCircle`, `DrawCircleLines`, `DrawRectangle`, `DrawRectangleLines`,
  `DrawRectangleRounded`, `DrawRectangleRoundedLines`, `DrawText`,
  `MeasureText`, `TextFormat`

libplayos:

- `playos_lifecycle_poll()`, `playos_lifecycle_wait()`
  (TERMINATE / BACKGROUND / FOREGROUND / SUSPEND / RESUME)
- `playos_input_get_controller_state()`, `playos_input_button_down()`
- `playos_log()` (via `PLAYOS_LOG_*` macros)

## Build

The on-device binary is cross-compiled by the `playos-samples` Buildroot
package and links `-lraylib -lplayos -lm` (see
`br2-external/package/playos-samples/playos-samples.mk`).

For a host development build, install raylib dev files and point `RAYLIB_DIR`
at them, then:

```sh
cmake -B build -DRAYLIB_DIR=/path/to/raylib/install
cmake --build build
```

## Run

```sh
./build/bin/game
```
