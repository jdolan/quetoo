# Contributing to Quetoo

Thank you for your interest in contributing to Quetoo! This guide covers how to build the engine, follow the code style, submit pull requests, and report bugs.

## Table of Contents

- [Building from Source](#building-from-source)
- [Code Style](#code-style)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Reporting Bugs](#reporting-bugs)
- [Architecture Overview](#architecture-overview)
- [Getting Help](#getting-help)

---

## Building from Source

### Dependencies

Install these libraries via your system package manager or Homebrew:

- [ObjectivelyMVC](https://github.com/jdolan/ObjectivelyMVC/)
- [PhysicsFS](https://icculus.org/physfs/)
- [OpenAL](https://www.openal.org/)
- [libsndfile](http://mega-nerd.com/libsndfile/)
- [glib2](https://developer.gnome.org/glib/)
- [ncurses](https://www.gnu.org/software/ncurses/)
- [SDL2](https://www.libsdl.org/)
- [libcurl](https://curl.se/libcurl/)

### Build Steps

```bash
# Clone and enter the repo
git clone https://github.com/jdolan/quetoo.git
cd quetoo

# Generate build system (only needed once after clone or configure.ac changes)
autoreconf -i

# Configure (standard build)
./configure

# macOS with Homebrew in non-standard prefix
./configure --with-homebrew=/opt/homebrew

# Build with tests
./configure --with-tests

# Compile
make -j$(nproc)

# Install
sudo make install
```

### Installing Game Data

The engine requires game data from a separate repository:

```bash
git clone https://github.com/jdolan/quetoo-data.git
sudo ln -s $(pwd)/quetoo-data/target /usr/local/share/quetoo
```

### Running Tests

```bash
./configure --with-tests
make -j$(nproc)
make check
```

---

## Code Style

Quetoo is written in C. Please follow these conventions when contributing code.

### Indentation

- **2 spaces** — no tabs.

### Naming Conventions

| Category | Convention | Example |
|---|---|---|
| Types/structs | `Snake_Case` with `_t` suffix | `entity_state_t`, `vec3_t` |
| Functions | `camelCase` | `G_Damage()`, `R_DrawBspModel()` |
| Variables | `camelCase` | `numEntities`, `frameTime` |
| Constants/macros | `ALL_CAPS` | `MAX_CLIENTS`, `CVAR_ARCHIVE` |
| Enum values | `ALL_CAPS` | `PHYSICS_TOSS`, `ERROR_DROP` |

### Module Prefixes

Each subsystem uses a consistent file and function prefix:

| Prefix | Subsystem |
|---|---|
| `r_` | Renderer (`src/client/renderer/`) |
| `cg_` | Client game module (`src/cgame/`) |
| `g_` | Server game module (`src/game/`) |
| `sv_` | Server (`src/server/`) |
| `cl_` | Client (`src/client/`) |
| `cm_` | Collision model (`src/collision/`) |
| `com_` | Common (`src/common/`) |
| `net_` | Network (`src/net/`) |

### General Rules

- Opening braces go on the **same line** for functions and control structures.
- Always use braces for `if`/`else`/`for`/`while` bodies, even single-line ones.
- Comment code that needs clarification; don't over-comment obvious logic.
- Keep functions short and focused on one task.
- Use `Mem_Malloc()` / `Mem_Free()` rather than `malloc()` / `free()` directly.
- Use `g_snprintf()` instead of `snprintf()` for internal string formatting.

### Vector Math

Use the provided vector types and helpers from `src/shared/`:

```c
// vec3_t is float[3]
vec3_t origin = Vec3(1.0f, 2.0f, 3.0f);
vec3_t copy = Vec3_Copy(origin);
float len = Vec3_Length(origin);
```

---

## Submitting a Pull Request

1. **Fork** the repository and create a branch off `main`:
   ```bash
   git checkout -b fix/my-bug-fix
   ```

2. **Keep commits clean** — squash fixups before opening the PR. Each commit should represent a logical unit of change.

3. **Reference issue numbers** in your commit messages and PR description:
   ```
   Fix frustum culling with half-FOV angle (#712)
   ```

4. **Run the tests** before submitting:
   ```bash
   make check
   ```

5. **Open the PR** against the `main` branch. Fill out the PR template completely.

6. Be responsive to review feedback. We aim to review PRs within a few days.

---

## Reporting Bugs

Use [GitHub Issues](https://github.com/jdolan/quetoo/issues) to report bugs.

A good bug report includes:

- **Operating system and version** (e.g., macOS 14.5, Ubuntu 24.04, Windows 11)
- **Hardware** — CPU, GPU, and driver version if it's a rendering issue
- **Quetoo version or commit hash**
- **Steps to reproduce** — the more specific, the better
- **Expected behavior** vs. **actual behavior**
- **Logs or console output** — run with `developer 1` for verbose output
- **Screenshots or video** if it's a visual issue

Please search existing issues before opening a new one.

---

## Architecture Overview

Quetoo has a modular architecture. Here's a brief map of the codebase:

```
src/
├── shared/      # Math, vectors, data structures shared by all modules
├── common/      # Console, cvars, filesystem, memory, threading
├── collision/   # BSP map loading and collision detection
├── net/         # UDP networking and delta compression
├── server/      # Game server host
├── game/        # Server-side game logic module (game.so / game.dll)
├── client/      # Client application (rendering, input, sound)
├── cgame/       # Client-side game logic module (cgame.so / cgame.dll)
├── ai/          # Bot AI system
├── main/        # Main executable entry points
└── tools/       # Map compiler (quemap) and other tools
```

Detailed documentation for each subsystem lives in [`.github/subsystems/`](.github/subsystems/README.md).

---

## Getting Help

- **Discord**: [https://discord.gg/unb9U4b](https://discord.gg/unb9U4b) — the fastest way to get help
- **GitHub Discussions**: For longer-form questions about the codebase
- **GitHub Issues**: For confirmed bugs and feature requests only
