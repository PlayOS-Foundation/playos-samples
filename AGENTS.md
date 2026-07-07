# AGENTS.md — playos-samples

Guidance for AI agents and contributors working in this repository.

## What this repository is

Official sample applications, tutorials, and reference projects that
demonstrate best practices for developing with the **PlayOS Platform API**.
The API contract is defined in `playos-spec`; samples show how to use it.

## Golden rules

1. **Show the intended developer experience.** Samples use Raylib for
   rendering + the PlayOS Platform API for platform services
   (`#include "raylib.h"` + `#include "playos/playos.h"`).
2. **Engine-agnostic where it counts.** Platform interactions go through
   `PlayOS::` APIs, not raw OS calls, so the sample is portable.
3. **Small and readable.** Samples teach; prefer clarity over cleverness.
4. **Keep it building** on Windows and Linux (Raylib via FetchContent; the
   Platform API resolved from a sibling checkout or GitHub).
5. **One concept per sample.** Add a new folder rather than overloading an
   existing sample.

## Layout

| Path | Purpose |
|---|---|
| `<sample>/src/` | Sample source |
| `CMakeLists.txt` | Builds all samples; fetches Raylib, links the Platform API |

## Adding a sample

1. Create `<sample>/src/main.cpp`.
2. Add an `add_executable` + `target_link_libraries(... PlayOS::PlatformAPI raylib)`.
3. Document it in `README.md`.
