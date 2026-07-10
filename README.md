# playos-samples

Official sample applications, tutorials, and reference projects demonstrating
best practices for developing with the **PlayOS Platform API**.

## Samples

| Sample | Description |
|---|---|
| `hello-playos` | The reference "your first PlayOS app": Raylib for rendering + the PlayOS Platform API for input and lifecycle. Press **B** / **Esc** to exit. |
| `space-invaders` | Classic arcade shooter: defend Earth from rows of aliens. Arrow keys to move, Space to shoot, Esc to quit. |

## Building

Requires CMake >= 3.20, a C++17 compiler, and network access (Raylib is
fetched via `FetchContent`; `playos-platform-api` is resolved from a sibling
checkout if present, otherwise from GitHub).

```sh
cmake -B build
cmake --build build
./build/hello-playos
```

## Relationship to the shell

`hello-playos` is a real PlayOS-style game that the
[`playos-shell`](https://github.com/PlayOS-Foundation/playos-shell) can launch
to demonstrate the console loop (`Shell -> Game -> Shell`). When built in the
standard sibling layout, the shell locates and launches it automatically.

## License

Sample code will be released under an OSI-approved license (MIT/Apache-2.0).
