# com.playos.sample-rotating-squares

Raylib sample that renders a field of rotating squares through the shared
`libraylib` (PlayOS Wayland/EGL/GLES2 backend).

This demonstrates the intended engine split on PlayOS: **Raylib owns
rendering**, while **libplayos owns lifecycle and input**. The PlayOS raylib
backend never feeds `WindowShouldClose()`, so the main loop exits through
`playos_lifecycle_poll()` (TERMINATE); the B button is also wired as an
in-game quit.

## API surface used

Raylib:

- `InitWindow`, `GetScreenWidth`, `GetScreenHeight`
- `BeginDrawing` / `ClearBackground` / `DrawRectanglePro` / `ColorFromHSV`
- `EndDrawing`, `CloseWindow`, `GetTime`

libplayos:

- `playos_lifecycle_poll()` (TERMINATE / BACKGROUND / FOREGROUND / SUSPEND / RESUME)
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
