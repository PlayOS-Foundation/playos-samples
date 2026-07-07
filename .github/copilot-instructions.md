# Copilot instructions — playos-samples

Official PlayOS samples and tutorials. They demonstrate using the **PlayOS
Platform API** (with Raylib for rendering). The API is defined in
[`playos-spec`](https://github.com/PlayOS-Foundation/playos-spec). Also read
`AGENTS.md`.

## Rules for changes here

1. Samples use Raylib for rendering + `PlayOS::` APIs for platform services.
2. Keep samples small, readable, and focused on one concept.
3. Platform interactions go through the Platform API, not raw OS calls.
4. Keep it building on Windows and Linux.
5. New concept = new folder + a `README.md` entry.
