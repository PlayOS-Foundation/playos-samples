# Fog Rendering

A PlayOS port of raylib's [`shaders_fog_rendering`](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_fog_rendering.c) example.

A torus, a cube and a sphere are lit by a single white point light and wrapped in exponential distance fog, all computed per-pixel in a GLSL ES 3.00 fragment shader. A line of tori fading into the distance makes the fog depth obvious. The camera auto-orbits around the scene, so no look controls are needed.

## Controls

| Input            | Action                    |
|------------------|---------------------------|
| D-pad up/down    | Increase / decrease fog density |
| KEY_UP/KEY_DOWN (desktop) | Increase / decrease fog density |
| B                | Quit                      |

## Shader notes

The PlayOS raylib backend is OpenGL ES 3.0, so `resources/lighting.vs` and `resources/fog.fs` are written in **GLSL ES 3.00** (`#version 300 es`, `in`/`out` qualifiers, `texture()`, `precision mediump float;`) — matching the backend — rather than the upstream example's desktop GLSL 330 / mobile GLSL 100 pairs.

Differences from the upstream `glsl330/fog.fs`:

- `#version 330` → `#version 300 es` + `precision mediump float;`.
- The unused `MaterialProperty` struct is removed — ES 3.00 does not allow `sampler2D` members inside a struct that is not a uniform block, and it is dead code upstream.

Uniforms `matModel`/`matNormal` are auto-updated by raylib during `DrawModel`; `viewPos` is set manually each frame (the upstream example does the same). The point light is created via `rlights.h` (`CreateLight`), whose `lights[0].{enabled,type,position,target,color}` uniforms are resolved by name.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib model/shader API — `LoadModelFromMesh`, `GenMeshTorus/Cube/Sphere`, `LoadShader`, `GetShaderLocation`, `SetShaderValue`, `CreateLight` (rlights), `BeginMode3D`/`EndMode3D`, `DrawModel`, `UpdateCamera`.
- raylib gamepad input — `IsGamepadAvailable`, `IsGamepadButtonPressed`/`_Down`.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```

Output binary: `build/bin/game`.
