# Spotlight

A PlayOS port of raylib's [`shaders_spotlight_rendering`](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_spotlight_rendering.c) example.

A GLSL fragment shader punches alpha "holes" into a full-screen black overlay, giving a top-down stealth look: the left half of the screen is pitch black except where the spotlights fall, and the right half is dimly lit. A star field and a ring of orbiting bobs make the lighting effect easy to read.

## Controls

| Input            | Action                  |
|------------------|-------------------------|
| Left stick/D-pad | Move the player spotlight |
| Mouse (desktop)  | Move the player spotlight |
| B                | Quit                    |

Spots 1 and 2 drift on their own and bounce off the edges.

## Shader notes

The PlayOS raylib backend is OpenGL ES 3.0, so `resources/spotlight.fs` is written in **GLSL ES 3.00** (`#version 300 es`, `out vec4 finalColor`, `precision mediump float;`) — matching the backend's default vertex shader — rather than the upstream example's desktop GLSL 330 / mobile GLSL 100 pair. The uniform array `spots[3]` and `screenWidth` are set each frame via `SetShaderValue`.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib shader API — `LoadShader`, `GetShaderLocation`, `SetShaderValue`, `BeginShaderMode`/`EndShaderMode`, `UnloadShader`.
- raylib gamepad input — `IsGamepadAvailable`, `IsGamepadButtonPressed`/`_Down`, `GetGamepadAxisMovement`.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
