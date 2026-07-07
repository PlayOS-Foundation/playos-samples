---
name: Implementation Task
about: A scoped task an AI agent or contributor can implement.
title: "Task: "
labels: [implementation, agent-ready]
assignees: []
---

## Goal

<!-- One sentence: what should exist after this task is complete. -->

## Source of truth (read first)

- `AGENTS.md`
- `.github/copilot-instructions.md`
- Relevant `playos-spec` chapters (e.g. Developer Guide, Platform API)

## Required output

<!-- Which sample to add/modify and what it demonstrates. -->

## Acceptance criteria

- [ ] Builds: `cmake -B build && cmake --build build`
- [ ] Uses Raylib for rendering + the PlayOS Platform API for platform services
- [ ] Small, readable, focused on one concept
- [ ] Documented in `README.md`

## Constraints

- Follow this repository's `AGENTS.md`.
- Keep the change scoped to this task.
