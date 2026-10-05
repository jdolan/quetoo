---
name: ci-watch-fix
description: Watch Quetoo's GitHub Actions CI on a branch or pull request, extract the real compiler and test errors from a failed run, diagnose them, and push a fix. Use when CI fails or after a push that needs confirming.
---

# Watch CI and fix failures

Use this skill when CI fails on a Quetoo branch or pull request, or after a push that you need to
confirm.

## What CI runs

`.github/workflows/build.yml` runs on pushes and pull requests to `main`.

| Job | Runner | What it does |
|---|---|---|
| `resolve-deps` | `ubuntu-latest` | Records the head commits of Objectively and ObjectivelyMVC. Both build jobs wait for it. |
| `build-linux` | `ubuntu-22.04` | Builds OpenAL Soft, the jdolan/SDL fork (tag `ObjectivelyGPU`), SDL3_image, SDL3_ttf, Objectively, ObjectivelyGPU and ObjectivelyMVC from source, then Quetoo with gcc and autotools. Runs `make check`, and packages `.deb`, `.rpm` and a tarball. |
| `build-windows` | `windows-latest` | Builds `Quetoo.vs15/quetoo_all.sln` with MSBuild and `ClangCL`. Gets SDL3 from the fork's release through ObjectivelyGPU's `sdl3.targets`. |
| `release` | `ubuntu-latest` | **Only on a push to `main`.** Uploads a `version` file to S3, and replaces the `latest` snapshot release with the new builds. |

- **A push to `main` publishes a player snapshot.** You MUST NOT push to `main` without the user's
  explicit approval.
- **CI checks out Objectively, ObjectivelyGPU and ObjectivelyMVC at the head of their `main`.** A
  fix in one of them reaches CI only after it is pushed to that repository's `main`. Before a
  Quetoo push that depends on one, check it with
  `git -C ../ObjectivelyMVC log origin/main..main --oneline`.
- CI is the only build of Linux and of Windows. Code behind `#if defined(_WIN32)` or
  `#elif defined(__linux__)` is compiled nowhere else.

## 1. Find the run

```sh
gh run list --branch "$(git branch --show-current)" --limit 5
```

## 2. Wait for it

```sh
gh run watch <RUN_ID> --exit-status
```

## 3. Extract the errors

`gh run view --log-failed` is very long. Filter it.

All real errors, both platforms:

```sh
gh run view <RUN_ID> --log-failed 2>&1 \
  | grep -E ':[0-9]+:[0-9]+: (fatal )?error:|\([0-9]+,[0-9]+\): (fatal )?error' \
  | sed -E 's/^.*[0-9]{2}\.[0-9]+Z //' | sort -u | head -40
```

Which job and step failed:

```sh
gh run view <RUN_ID>
```

Windows project that failed:

```sh
gh run view <RUN_ID> --log-failed 2>&1 | grep build-windows | grep -E 'FAILED|error MSB' | head
```

Unit test output (Linux): the `make check` step prints `src/tests/test-suite.log` when a suite
fails.

## 4. Diagnose

- **Undeclared identifier, or a call to an undeclared function.** A file uses a POSIX or C library
  symbol without its header. Common gaps on Windows: `<signal.h>`, and `<strings.h>` for
  `strcasecmp`. Do not call `strtok_r`, which MSVC does not have: use `Str_Tokenize` from
  `shared/qstring.h`.
- **No member named X, or a wrong number of arguments for an Objectively call.** A sibling library
  on GitHub differs from the local one. Push it, or rebase on it.
- **Undefined reference, or `LNK2019`.** A new source file is missing from a build system. On
  Windows it is usually missing from `Quetoo.vs15/libs/lib<name>.vcxproj`, `cgame_common.props` or
  `game_common.props`. See `AGENTS.md`, "Building", for the full list.
- **Undefined `SDL_*GPUQuery*` symbol, or the warning `SDL3 lacks SDL_GPU_QUERY_API`.** The job built
  against stock SDL. See `AGENTS.md`, "SDL3 is a fork".
- **A crash in the crash handler (`Sys_Raise`, `Sys_Backtrace`).** Code there MUST use `malloc` and
  `free`, never `Mem_Malloc` or `Mem_Free`.

## 5. Fix and verify locally

```sh
make -j$(sysctl -n hw.logicalcpu) 2>&1 | grep -E 'error:' | head
make check
```

Commit with the attribution trailer that your harness requires. You MUST NOT push without the user's
approval. A push to `main` also publishes a player snapshot.

## 6. Confirm

```sh
gh run list --branch "$(git branch --show-current)" --limit 3
gh run watch <NEW_RUN_ID> --exit-status
```

Repeat until the run is green.
