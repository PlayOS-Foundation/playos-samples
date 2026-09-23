# PlayOS SDK Reference

A minimal PlayOS game built **entirely with the PlayOS SDK** — no Buildroot tree,
no component sources. It is the sample used to validate the three SDK build
profiles (`device`, `desktop`, `emulator`) and to show what a third-party game
looks like.

One `src/main.c` builds for every profile; nothing is forked per target.

## Build and run

```sh
export PLAYOS_SDK=/path/to/playos-tools/sdk

# Device (musl) — shippable artifact
sdk/scripts/build-device.sh sdk-reference

# Desktop (native window)
sdk/scripts/build-desktop.sh sdk-reference

# Emulator (device build in QEMU) — needs a built emulator image:
#   (cd playos-refdistro && make emulator-build)
PLAYOS_REFDISTRO=/path/to/playos-refdistro \
    sdk/scripts/build-emulator.sh sdk-reference
```

## API surface exercised

| Module | Calls | Why |
|---|---|---|
| system | `playos_system_api_version`, `_os_version`, `_device_model`, `_gpu_description` | ABI check + identity in the HUD |
| lifecycle | `playos_lifecycle_poll` | Foreground/background/suspend/resume/terminate, with near-zero CPU while hidden |
| input | `playos_input_controller_connected`, `playos_input_get_controller_state`, `playos_input_button_down` | Left stick moves the marker; B exits this sample |
| storage | `playos_storage_get_saves_path`, `playos_storage_free_bytes`, `playos_storage_atomic_write` | Per-game saves; run counter written atomically on suspend/terminate |
| logging | `playos_log` (`PLAYOS_LOG_I/W`) | Boot, save and lifecycle lines |
| raylib | `InitWindow`, `BeginDrawing`/`EndDrawing`, `DrawRectanglePro`, `DrawText`, `TextFormat` … | Animated scene, so the compositor has frames to present |

Nothing touches `/run/playos`, DRM/KMS, or compositor internals — the public
`libplayos` API only.

## Per-profile validation (2026-09-22)

| Profile | Build | Run | Evidence |
|---|---|---|---|
| `device` | **OK** — musl, `ld-musl-x86_64.so.1`, via the SDK toolchain | not run on an Ally in this session | `file bin/game` shows the musl interpreter |
| `desktop` | **OK** — glibc x86-64, links the SDK's desktop `libraylib.so.6` + `libplayos.so.0` | deferred: no `DISPLAY`/`WAYLAND_DISPLAY` in this session | build output in the table row above |
| `emulator` | **OK** — same musl artifact | **PASS** | see below |

`emulator` (the device artifact booted in the minimal PlayOS QEMU image with
`playos.autostart=com.playos.sdk-reference`):

- init: `autostart requested - launching game com.playos.sdk-reference` →
  `spawning game: … (/data/games/com.playos.sdk-reference/bin/game)`, under the
  Sprint 12 sandbox.
- game: `sdk-reference 1.0.0 starting (libplayos 1, os unknown)`;
  `saves at /data/saves/com.playos.sdk-reference (0 previous run(s), 473092096 bytes free)`;
  raylib `Platform backend: PLAYOS (Wayland + EGL/GLES2)`,
  `DISPLAY: Device initialized successfully` (Mesa softpipe, GLES 3.1);
  lifecycle `event: FOREGROUND`.
- compositor: `game surface added to scene (role 3)`, `fps shell=0 game=1`.
- the runner exits 0 with all three checks `[ok]`.

## Notes

- **`os unknown` on the emulator** is what `playos_system_os_version()` returns
  in that minimal image; the HUD prints it verbatim. The API call itself works.
- **The run counter is not visible in the emulator run** because QEMU is killed
  on the timeout, so no `SUSPEND`/`TERMINATE` arrives and the save is never
  written. The storage read path (`0 previous run(s)`, free bytes) is exercised.
- **The desktop window needs a display and (for Wayland) a Wayland-capable
  desktop raylib.** `export-sdk.sh` warns when GLFW built X11-only because
  `libdecor-0-dev` is missing; `apt install libdecor-0-dev libxkbcommon-dev
  wayland-protocols` fixes it.
- **B exits this sample** so a tester can leave it with only a controller. B is
  not a reserved key; a real game may use it however it likes — quitting is the
  shell overlay's Quit Game (`PLAYOS_LIFECYCLE_TERMINATE`).
