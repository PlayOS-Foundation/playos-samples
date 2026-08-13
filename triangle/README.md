# com.playos.sample-triangle

Minimal PlayOS sample that reports system and display information through the
public Platform API.

## API surface used

- `playos_system_api_version()`
- `playos_display_get_info()`
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

Note: actual hardware-accelerated triangle rendering is wired in a later
sprint. This binary proves the launch + display-query path and exits cleanly.
