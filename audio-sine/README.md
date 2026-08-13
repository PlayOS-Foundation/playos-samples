# com.playos.sample-audio

PlayOS audio sample placeholder. Reports current audio device state; sine-tone
playback is wired in Sprint 8.

## API surface used

- `playos_audio_get_info()`
- `playos_lifecycle_wait()`
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
