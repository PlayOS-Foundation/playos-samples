# Invaders

A complete single-screen arcade shooter for PlayOS — a reference **full game**
(not a one-feature demo), built with `libplayos` + raylib and shipped in the
reference image's game library.

Five rows of invaders march side to side, drop when they reach an edge, and
speed up as the fleet thins. You move, fire, and hide behind four eroding
shields while they drop bombs. Clear the fleet to advance a level; three lives;
the high score persists across runs.

## Controls

| Action | Controller | Keyboard (desktop build) |
|---|---|---|
| Move | left stick or D-pad | ← / → or A / D |
| Fire | A (South) | Space |
| Restart (game over) | Start | Enter |

## API surface exercised

| Module | Calls | Why |
|---|---|---|
| system | `playos_system_api_version`, `_os_version` | ABI check + boot log |
| lifecycle | `playos_lifecycle_poll` | Foreground/background pause, save + exit on terminate |
| input | `playos_input_controller_connected`, `playos_input_get_controller_state`, `playos_input_button_down` | Movement, fire (edge-detected), restart |
| storage | `playos_storage_get_saves_path`, `playos_storage_atomic_write` | `highscore.txt`, written atomically |
| logging | `playos_log` | Boot, life lost, game over, save path |
| raylib | `InitWindow`, `Camera2D` scale-to-display, `DrawRectangle`, `MeasureText`, `GetFrameTime` … | Rendering, fixed 1280×720 virtual playfield |

Everything lives in one state struct; there is no allocation after startup and
nothing touches compositor internals or the control socket.

## Build and run

```sh
export PLAYOS_SDK=/path/to/playos-tools/sdk

sdk/scripts/build-device.sh invaders            # musl, shippable
sdk/scripts/build-desktop.sh invaders           # native window (keyboard)
PLAYOS_REFDISTRO=/path/to/playos-refdistro \
    sdk/scripts/build-emulator.sh invaders       # device build in QEMU
```

It also ships in the image: `playos-samples.mk` builds it with the target
toolchain and installs it to `/usr/share/playos/games/com.playos.sample-invaders`,
which `playos-init` seeds into `/data/games/` on first boot.

## Validation (2026-09-22)

| Profile | Build | Run |
|---|---|---|
| `device` | **OK** — musl (`ld-musl-x86_64.so.1`) via the SDK | not run on an Ally in this session |
| `desktop` | **OK** — glibc x86-64, links the SDK's `libraylib.so.6` + `libplayos.so.0` | deferred: no `DISPLAY`/`WAYLAND_DISPLAY` in the session |
| `emulator` | **OK** — same musl artifact | **launched and claimed as the game surface** |

Emulator evidence: init `autostart requested - launching game
com.playos.sample-invaders` → `spawning game: …/bin/game`; the game logs
`invaders starting (libplayos 1, …)`, `saves at /data/saves/com.playos.sample-invaders`,
raylib `Platform backend: PLAYOS (Wayland + EGL/GLES2)`, `DISPLAY: Device
initialized successfully` (Mesa softpipe) and `lifecycle event: FOREGROUND`; the
compositor logs `game surface added to scene (role 3)`.

**Caveat:** this host has no readable `/dev/kvm` (`root:kvm`, the user is not in
the group), so QEMU fell back to TCG. Under TCG the guest clock runs ~4× slower
than wall time and softpipe never completed a frame of the full-size output
before the timeout, so the compositor's `fps … game=N` commit-rate sample did not
appear. On a KVM host the same path produced `game=1` for the SDK reference.
To reproduce with commit-rate evidence:

```sh
sudo usermod -aG kvm "$USER"   # then re-login
PLAYOS_REFDISTRO=/path/to/playos-refdistro sdk/scripts/build-emulator.sh invaders
```

Input in the emulator needs a real device passed through
(`build-emulator.sh invaders build-emu --gamepad /dev/input/eventN`); QEMU's
synthetic keyboard is not a gamepad, so the device input backend honestly
reports "no controller" and the ship stays put while the fleet marches.

## Frame pacing and the periodic stutter

On hardware (ROG Ally) the game hitched roughly every few seconds. Measured
cause (**not** the game, and not frame pacing): `playos-platform-api`'s evdev
backend re-scans `/dev/input/event0-31` whenever a device class is missing, and
the Ally has no BTN_MODE "home" node — so it re-scans every 2 s
(`RESCAN_INTERVAL_US`). Each scan **opens and closes up to 32 evdev nodes**, and
`close()` on an evdev fd runs `input_close_device()` → `synchronize_rcu()`:

```
__wait_rcu_gp ← synchronize_rcu_normal ← input_close_device ← evdev_release ← __fput ← close(2)
```

~30 ms per close × ~12 nodes ≈ **a ~0.4 s stall every 2 s**, inside the game
process. That also explains the compositor's `100 ↔ 30` presents/s alternation
(the client commits nothing during the stall). Fix belongs in the input backend
(enumerate via sysfs instead of opening every node, and back the rescan off).


## Backgrounding: never skip `EndDrawing()`

A backgrounded game **must keep calling `EndDrawing()`** — i.e. keep drawing and
committing a frame. On the PlayOS raylib backend `PollInputEvents()`, which is
what reads the Wayland socket, is called *inside* `EndDrawing()`. So a "pause"
loop like

```c
if (paused) { WaitTime(0.05); continue; }   /* WRONG — kills the game */
```

stops servicing the compositor entirely. Its requests queue unread, and roughly
1.5 s later the compositor kills the client:

```
[shell] ARMOURY CRATE tap - showing overlay
[shell] async: game crashed                     ← 1.6 s later, no clean exit
```

The overlay/exit flow then never completes: the screen is left on the last frame
with no way to quit (hit twice on the ROG Ally, 2026-09-22, requiring hard
reboots). The game had never logged `exiting after … frames` in any session.

The fix is to gate only the **simulation** on `paused` and always fall through to
draw and `EndDrawing()`:

```c
if (!g.paused) { update_play(dt, in); }   /* skip the game, not the frame */
... draw ...
EndDrawing();      /* swaps buffers AND pumps Wayland events */
```


