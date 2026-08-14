# com.playos.sample-controller-visualizer

Raylib sample that renders an on-screen gamepad and lights each control as the
player actuates it — a PlayOS port of raylib's
[`core_input_gamepad`](https://github.com/raysan5/raylib/blob/master/examples/core/core_input_gamepad.c)
example.

This demonstrates the intended engine split on PlayOS: **Raylib owns
rendering and gamepad input**, while **libplayos owns lifecycle**. The PlayOS
raylib backend feeds raylib's native gamepad API (`IsGamepadAvailable`,
`IsGamepadButtonDown`, `GetGamepadAxisMovement`) from libplayos'
hardware-agnostic logical controller API, so the sample uses the standard
raylib gamepad API exactly like upstream `core_input_gamepad`. It drops
gamepad enumeration, vibration, keyboard, and mouse handling (PlayOS exposes
a single logical controller). The gamepad itself is drawn programmatically
(no texture assets), and trigger axes — raylib's `[-1,1]` (rest = -1) — are
remapped to `[0,1]` (rest = 0) for the trigger bars.

The main loop exits through `playos_lifecycle_poll()` (TERMINATE); the B
button is also wired as an in-game quit.

## Button / axis mapping

| Control | raylib input |
|---|---|
| A / B / X / Y | `GAMEPAD_BUTTON_RIGHT_FACE_DOWN` / `RIGHT` / `LEFT` / `UP` |
| D-pad | `GAMEPAD_BUTTON_LEFT_FACE_UP` / `DOWN` / `LEFT` / `RIGHT` |
| L1 / R1 | `GAMEPAD_BUTTON_LEFT_TRIGGER_1` / `RIGHT_TRIGGER_1` |
| L3 / R3 (stick click) | `GAMEPAD_BUTTON_LEFT_THUMB` / `RIGHT_THUMB` |
| Select / Start | `GAMEPAD_BUTTON_MIDDLE_LEFT` / `MIDDLE_RIGHT` |
| Left stick | `GAMEPAD_AXIS_LEFT_X`, `GAMEPAD_AXIS_LEFT_Y` (deadzone 0.10) |
| Right stick | `GAMEPAD_AXIS_RIGHT_X`, `GAMEPAD_AXIS_RIGHT_Y` (deadzone 0.10) |
| L2 / R2 (analog) | `GAMEPAD_AXIS_LEFT_TRIGGER`, `GAMEPAD_AXIS_RIGHT_TRIGGER` (`[-1,1]`, remapped to `[0,1]`) |

## API surface used

Raylib:

- `InitWindow`, `BeginDrawing` / `ClearBackground`, `EndDrawing`, `CloseWindow`
- `IsGamepadAvailable`, `GetGamepadName`, `IsGamepadButtonDown`,
  `IsGamepadButtonPressed`, `GetGamepadAxisMovement`
- `DrawCircle`, `DrawCircleLines`, `DrawRectangle`, `DrawRectangleLines`,
  `DrawRectangleRounded`, `DrawRectangleRoundedLines`, `DrawText`,
  `MeasureText`, `TextFormat`

libplayos:

- `playos_lifecycle_poll()`, `playos_lifecycle_wait()`
  (TERMINATE / BACKGROUND / FOREGROUND / SUSPEND / RESUME)
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
