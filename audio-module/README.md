# com.playos.sample-audio-module

A PlayOS port of raylib's `audio_module_playing` example. Streams the
FastTracker II module `resources/mini1111.xm` through raylib's jar_xm backend
and renders a pulsing-circle visualizer plus a playback progress bar.

## Controls

| Input    | Action                        |
|----------|-------------------------------|
| A        | Restart the module            |
| X        | Pause / resume                |
| D-pad ↑↓ | Raise / lower pitch (speed)   |
| B        | Quit                          |

The upstream keyboard bindings (SPACE / P / UP / DOWN) are also accepted for
desktop builds; on device the PlayOS raylib backend never feeds keyboard state.

## Notes

- The module ships under `resources/` and is resolved relative to the game's
  working directory (`/data/games/<app-id>/`), which playos-init seeds from
  `/usr/share/playos/games/<app-id>/` on first boot.
- Lifecycle drives pause/resume (BACKGROUND/SUSPEND pauses, FOREGROUND/RESUME
  resumes, TERMINATE exits); the raylib backend never feeds `WindowShouldClose()`.
- Under QEMU/CI with no audio hardware, `IsAudioDeviceReady()` is false and the
  visualizer still renders with an "audio unavailable" status.

## API surface used

- raylib `LoadMusicStream` / `PlayMusicStream` / `UpdateMusicStream` /
  `PauseMusicStream` / `ResumeMusicStream` / `SetMusicPitch` / `GetMusicTimePlayed`
- raylib native gamepad API (`IsGamepadAvailable`, `IsGamepadButtonPressed`,
  `IsGamepadButtonDown`)
- `playos_lifecycle_wait()` / `playos_lifecycle_poll()`
- `playos_log()` (via `PLAYOS_LOG_*` macros)

## Build

```sh
cmake -B build
cmake --build build
```

## Run

```sh
./build/bin/game
```
