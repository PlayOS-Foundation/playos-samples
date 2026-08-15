# Colors Palette

A PlayOS port of raylib's [`shapes_colors_palette`](https://github.com/raysan5/raylib/blob/master/examples/shapes/shapes_colors_palette.c) example.

Renders all 21 built-in raylib colors as an interactive 7×3 grid and lets the player inspect each color's name. The upstream example is mouse-driven; on PlayOS the pointer is replaced with a gamepad cursor.

## Controls

| Input           | Action                          |
|-----------------|---------------------------------|
| D-pad           | Move the selection cursor       |
| A               | Toggle showing every color name |
| B               | Quit                            |

Desktop fallback (inert on device): arrow keys move the cursor, `SPACE` toggles names, and mouse hover highlights the cell under the pointer.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib gamepad input (`IsGamepadAvailable`, `IsGamepadButtonPressed`) — the PlayOS raylib backend exposes one logical controller fed from `libplayos`.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
