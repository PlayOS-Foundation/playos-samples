# Bunnymark

A PlayOS port of raylib's [`textures_bunnymark`](https://github.com/raysan5/raylib/blob/master/examples/textures/textures_bunnymark.c) example.

Spawns up to 80,000 textured bunnies that bounce around the screen, reporting FPS and batched draw calls to measure renderer throughput. The scene is rendered into an 800×450 render texture and scaled letterboxed to the real surface, so the benchmark measures batching at the standard bunnymark resolution.

## Controls

| Input      | Action               |
|------------|----------------------|
| A (hold)   | Spawn bunnies        |
| X          | Toggle pause         |
| LB         | Clear all bunnies    |
| B          | Quit                 |

There is no pointer on device, so bunnies spawn at the scene center and fan out with random velocities (the upstream example spawns at the mouse position).

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib gamepad input (`IsGamepadAvailable`, `IsGamepadButtonPressed`/`_Down`) — the PlayOS raylib backend exposes one logical controller fed from `libplayos`.
- raylib `RenderTexture` — the scene canvas (`LoadRenderTexture`, `BeginTextureMode`/`EndTextureMode`, `UnloadRenderTexture`).

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
