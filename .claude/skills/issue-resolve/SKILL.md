---
name: issue-resolve
description: Work a Quetoo GitHub issue from triage to a merged pull request with green CI. Use when the user brings a GitHub issue number or link to work on.
---

# Resolve a GitHub issue

Use this skill when the user brings a GitHub issue to work on. It covers triage through a merged
pull request with green CI. `AGENTS.md` governs naming, build systems and the rules that fail
silently. Read it first.

## 1. Triage

```sh
gh issue view <NUMBER> --comments
```

Identify:

- **Type:** bug, regression, feature, performance or documentation.
- **Scope:** the subsystems involved, by prefix: renderer `R_`, client `Cl_`, client game `Cg_`,
  game `G_`, server `Sv_`, collision `Cm_`, `quemap`, or a sibling repository (see `AGENTS.md`,
  "Sibling repositories").
- **Reproduction:** a map, a demo, a crash dump or a screenshot.
- **Done:** what the user will accept.

If the issue has a Windows crash dump (`.dmp`, or a `.zip` that holds one), use the
`win-crash-debug` skill before you continue.

For a regression, search the history before you change code. Quetoo records its design decisions in
commit bodies: `git log -S '<symbol>'` and `git log --grep '<keyword>'`. A removal that looks like a
mistake can be deliberate.

`doc/copilot/` holds older investigation notes for the renderer, shadows and entity state. They can
be out of date. Verify any claim in them against the code.

## 2. Plan

If the change touches more than one file, or adds or removes a source file, write the plan and wait
for the user's approval before you edit anything. A one-file fix, such as a missing include or an
off-by-one, needs no approval.

## 3. Implement

- Follow `AGENTS.md`, "Naming". Case encodes a category: a callable is PascalCase, data is camelCase.
- Register a client-game cvar with `cgi.AddCvar(name, value, flags, description)`, and an engine
  cvar with `Cvar_Add`.
- Use `Mem_Malloc` and `Mem_Free` for engine allocations. Use `malloc` and `free` in the crash
  handler (`Sys_Raise`, `Sys_Backtrace`).
- Add a unit test under `src/tests` (libcheck) where the change can be tested. Follow the existing
  suites. A new test binary also goes in the root `.gitignore`, which lists every `src/tests/check_*`.
- A new source file goes in every build system: see `AGENTS.md`, "Building".

## 4. Build and test

```sh
make -j$(sysctl -n hw.logicalcpu) 2>&1 | grep -E 'error:' | head
make check
plutil -lint Quetoo.xcodeproj/project.pbxproj   # if the Xcode project changed
```

## 5. Commit and open a pull request

- Follow `CONTRIBUTING.md`. The subject is imperative, with no subsystem prefix, for example
  `Fix frustum culling with half-FOV angle`. Put the rationale in the body, and reference the issue:
  `Fixes #<NUMBER>`.
- Add the attribution trailer that your harness requires.
- Open the pull request against `main` with `gh pr create`, and include `Fixes #<NUMBER>` in the
  body.
- You MUST NOT push without the user's approval. A push to `main` publishes a player snapshot.

## 6. Watch CI

After the push, use the `ci-watch-fix` skill until CI is green.
