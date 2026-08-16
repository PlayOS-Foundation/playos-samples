# Cel Shading

A PlayOS port of raylib's [`shaders_cel_shading`](https://github.com/raysan5/raylib/blob/master/examples/shaders/shaders_cel_shading.c) example.

A GLB car model (`old_car_new.glb`) is rendered with a per-pixel toon shader: diffuse lighting is quantized into a user-adjustable number of discrete bands, giving the flat "cel" look. An inverted-hull pass renders the model's extruded back faces as a dark silhouette, producing a crisp outline. The camera auto-orbits (CAMERA_ORBITAL), and a directional light spins opposite to it so the banding is obvious as you watch.

## Controls

| Input                          | Action                              |
|--------------------------------|-------------------------------------|
| A / Z (desktop)                | Toggle cel shading on/off           |
| X / C (desktop)                | Toggle outline pass on/off          |
| D-pad up/down / E,Q (desktop)  | Step toon band count (2..20)        |
| B                              | Quit                                |

## Shader notes

The PlayOS raylib backend is OpenGL ES 3.0, so the shaders are written in **GLSL ES 3.00** (`#version 300 es`, `in`/`out` qualifiers, `texture()`, `precision mediump float;`) rather than the upstream example's desktop GLSL 330 / mobile GLSL 100 pairs.

Differences from the upstream `glsl330` shaders:

- `cel.vs` / `cel.fs` / `outline_hull.vs` / `outline_hull.fs` all use `#version 300 es` (+ `precision` in the fragment shaders).
- `cel.fs` keeps upstream's `struct Light` with a trailing `float attenuation;` member so its uniform layout matches `rlights.h`. The `attenuation` field is unused by the fragment shader but is required for member-name uniform resolution to line up. Dynamic indexing of the `lights[4]` uniform array (`lights[i]`) is allowed in ES 3.00.
- The toon quantization uses `min(floor(NdotL * numBands), numBands - 1.0)` to avoid an out-of-range band index when `NdotL == 1.0`.

Uniforms `mvp`/`matModel`/`matNormal` are auto-updated by raylib during `DrawModel`; `viewPos`, `numBands` and `outlineThickness` are set manually. The directional light is created via `rlights.h` (`CreateLight`), whose `lights[0].{enabled,type,position,target,color}` uniforms are resolved by name.

Unlike the upstream example (which only applies the custom shader to `model.materials[0]`), this port applies it to **every** material on the model so multi-material GLBs stay consistent.

## API surface

- `playos_lifecycle_poll()` / `playos_lifecycle_wait()` — drives the main loop; `TERMINATE` exits, `BACKGROUND`/`SUSPEND` idles, `FOREGROUND`/`RESUME` resumes.
- `PLAYOS_LOG_I` — structured logging.
- raylib model/shader API — `LoadModel` (GLB), `LoadShader`, `GetShaderLocation`, `SetShaderValue`, `CreateLight` (rlights), `BeginMode3D`/`EndMode3D`, `DrawModel`, `rlSetCullFace`, `UpdateCamera`.
- raylib gamepad input — `IsGamepadAvailable`, `IsGamepadButtonPressed`.

## Build

```sh
cmake -S . -B build \
  -DPLAYOS_PLATFORM_API_DIR=/path/to/playos-platform-api \
  -DRAYLIB_DIR=/path/to/raylib/build
cmake --build build
```
