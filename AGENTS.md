# AGENTS.md — playos-samples

> **Implementation status:** 🔴 Pre-implementation — repository scaffold only. No sample projects, games, or apps yet.

## What this repository is

Example games and applications for PlayOS, demonstrating correct use of `libplayos` (the public C ABI from `playos-platform-api`). These samples serve as reference implementations for game developers targeting the PlayOS platform.

## Golden Rules

1. **Use `libplayos` exclusively.** Samples must only call the public Platform API — never access `/run/playos/` IPC, DRM/KMS, or compositor internals.
2. **Engine-agnostic where possible.** Provide samples in raw C99 + libplayos. Engine-specific wrappers (Raylib, SDL2, etc.) are welcome as additional examples.
3. **Build with CMake.** Each sample should have its own `CMakeLists.txt` that links `libplayos`.
4. **Document the API surface used.** Each sample README must list which `playos_*` functions it exercises, so developers can use it as a learning reference.
5. **Spec first.** Sample behavior and API usage must conform to `playos-spec/src/platform-api.md`.

## Planned Samples

| Sample | Purpose | API Surface |
|---|---|---|
| `hello-playos` | Minimal init/lifecycle loop | `playos_system_api_version()`, `playos_lifecycle_poll()`, `playos_log()` |
| `input-debug` | Display all controller input events | `playos_input_get_controller_state()`, `PLAYOS_BUTTON_*` constants |
| `triangle` | Hardware-accelerated triangle via EGL/Wayland | `playos_display_get_info`, Wayland surface setup |
| `rotating-squares` | Rotating squares rendered via Raylib | Raylib render API + `playos_lifecycle_poll()`, `playos_input_get_controller_state()` |
| `controller-visualizer` | On-screen gamepad that lights controls as they're actuated (port of raylib `core_input_gamepad`) | Raylib render API + `playos_input_get_controller_state()`, `playos_input_button_down()` |
| `audio-sine` | Play a sine wave through ALSA | `playos_audio_open`, `playos_audio_write` |
| `audio-module` | Stream a FastTracker II (.xm) module through raylib with a pulsing-circle visualizer (port of raylib `audio_module_playing`) | Raylib `LoadMusicStream`/`UpdateMusicStream`/`SetMusicPitch` + `playos_lifecycle_poll()` |
| `colors-palette` | Interactive 7×3 grid of raylib's 21 built-in colors (port of raylib `shapes_colors_palette`) | Raylib `DrawRectangleRec`/`DrawRectangleLinesEx` + `playos_lifecycle_poll()`/`_wait()`, raylib gamepad input |
| `bullet-hell` | Spinning magic circle spraying a radial bullet fan with live spawner tuning (port of raylib `shapes_bullet_hell`) | Raylib `RenderTexture`/`DrawTexturePro`/`DrawCircleV` + `playos_lifecycle_poll()`/`_wait()`, raylib gamepad input |
| `save-game` | Read/write save data | `playos_storage_get_saves_path()`, `playos_storage_atomic_write()` |
| `full-game-template` | Complete game loop with all subsystems | All API modules |

## Repository Layout (target)

```
hello-playos/
├── README.md
├── CMakeLists.txt
└── src/
    └── main.c

input-debug/
├── README.md
├── CMakeLists.txt
└── src/
    └── main.c

triangle/
├── README.md
├── CMakeLists.txt
└── src/
    └── main.c

rotating-squares/
├── README.md
├── CMakeLists.txt
├── manifest.json
├── assets/
│   └── icon.png
└── src/
    └── main.c

... (one directory per sample)
```

## Code Conventions

- C99 for raw libplayos samples.
- Each sample is self-contained — no shared libraries between samples.
- `README.md` in each sample directory explains build, run, and API surface used.
- Use the stub backend (`-DPLAYOS_BACKEND=stub`) for host development and testing.

## Build Commands

```sh
# Per-sample build (example):
cd hello-playos
cmake -B build -DPLAYOS_BACKEND=stub
cmake --build build

# Run on host (stub backend):
./build/hello-playos
```

## What NOT to Do

- Do not add samples that bypass `libplayos` — no direct IPC, no raw DRM/KMS.
- Do not depend on systemd, PulseAudio, PipeWire, or desktop environment APIs.
- Do not add samples that require network access (Wi-Fi/cloud are post-MVP).
- Do not commit build artifacts — use `.gitignore`.
