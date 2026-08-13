# com.playos.sample-input

PlayOS sample that reports the current logical controller state.

## API surface used

- `playos_input_controller_connected()`
- `playos_input_get_controller_state()`
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
