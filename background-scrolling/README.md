# Background Scrolling

A PlayOS port of raylib's [`textures_background_scrolling`](https://github.com/raysan5/raylib/blob/master/examples/textures/textures_background_scrolling.c) example.

Three cyberpunk street layers (background, midground, foreground) scroll left at different speeds to produce a parallax depth effect. Each layer is scaled so its height fills the display and tiled horizontally to cover the viewport, so the scene fills a 1920×1080 (Ally) screen.

## Controls

| Input | Action |
|-------|--------|
| B     | Quit   |

This is a watch-only demo; the upstream example has no interaction beyond closing the window.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` / `PLAYOS_LOG_E` — structured logging.
- raylib gamepad input (`IsGamepadAvailable`, `IsGamepadButtonPressed`) — the PlayOS raylib backend exposes one logical controller fed from `libplayos`.
- raylib texture drawing (`LoadTexture`, `DrawTextureEx`, `UnloadTexture`) with three runtime resources under `resources/`.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
