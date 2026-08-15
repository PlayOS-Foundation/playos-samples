# Bullet Hell

A PlayOS port of raylib's [`shapes_bullet_hell`](https://github.com/raysan5/raylib/blob/master/examples/shapes/shapes_bullet_hell.c) example.

A spinning "magic circle" at the screen center continuously sprays a radial fan of bullets. Tune the spawner live to stress-test the renderer (up to 500,000 bullets). The scene is rendered into an 800×450 render texture and scaled letterboxed to the real surface.

## Controls

| Input      | Action                          |
|------------|---------------------------------|
| D-pad L/R  | Change bullet row count         |
| D-pad U/D  | Change bullet speed             |
| X / Y      | Decrease / increase cooldown    |
| A (hold)   | Advance spawn angle increment   |
| RB         | Toggle draw method              |
| LB         | Clear all bullets               |
| B          | Quit                            |

Desktop fallback (inert on device): `A`/`D` rows, `W`/`S` speed, `Z`/`X` cooldown, `SPACE` angle, `ENTER` draw method, `C` clear.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib gamepad input (`IsGamepadAvailable`, `IsGamepadButtonPressed`/`_Down`) — the PlayOS raylib backend exposes one logical controller fed from `libplayos`.
- raylib `RenderTexture` — the scene canvas (`LoadRenderTexture`, `BeginTextureMode`/`EndTextureMode`, `UnloadRenderTexture`) plus the per-bullet texture used for the fast draw path.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
