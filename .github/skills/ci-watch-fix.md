# Skill: CI Watch & Fix (ci-watch-fix)

Use this skill whenever CI is failing (or you've just pushed a fix and need to
confirm it landed) on a Quetoo PR or branch.

---

## CI Overview

Three jobs run on every push to a PR branch:

| Job | Runner | Toolchain |
|-----|--------|-----------|
| `resolve-deps` | ubuntu-latest | Resolves SHA pins for Objectively / ObjectivelyMVC |
| `build-linux` | ubuntu-22.04 | gcc / autotools |
| `build-windows` | windows-latest | clang-cl + MSBuild (`quetoo_all.sln`) |

Dependencies pulled from GitHub at HEAD of their `main` branch:
- `jdolan/Objectively`
- `jdolan/ObjectivelyMVC`

**Critical implication**: fixes to Objectively or ObjectivelyMVC must be pushed
to `jdolan/Objectively` / `jdolan/ObjectivelyMVC` before CI will pick them up.
The local `WickedOldGames` fork uses a dual-push remote; verify with:

```bash
cd ~/Coding/ObjectivelyMVC && git push origin main
git ls-remote https://github.com/jdolan/ObjectivelyMVC.git HEAD
```

---

## Step 1 — Find the current run

```bash
# Most recent runs for the current branch / PR
gh run list --branch $(git rev-parse --abbrev-ref HEAD) --limit 5
```

Note the run ID of the latest `in_progress` or `failure` run.

---

## Step 2 — Wait for a run to finish (if still in progress)

```bash
gh run watch <RUN_ID>
# or just poll:
gh run view <RUN_ID> | head -20
```

---

## Step 3 — Extract errors (the fast path)

`gh run view --log-failed` produces massive output. Use these targeted greps:

### Linux errors
```bash
gh run view <RUN_ID> --log-failed 2>&1 \
  | grep "build-linux" \
  | grep -E ":[0-9]+:[0-9]+: error:" \
  | head -30
```

Linux compiler errors have the form:
```
build-linux  UNKNOWN STEP  <timestamp>  file.c:420:12: error: 'SIGSEGV' undeclared
```

### Windows errors
```bash
gh run view <RUN_ID> --log-failed 2>&1 \
  | grep "build-windows" \
  | grep -E "error :" \
  | grep -v "warning\|cache\|runner\|Set up\|group" \
  | head -30
```

Windows (clang-cl) errors have the form:
```
build-windows  UNKNOWN STEP  <timestamp>  file.c(420,5): error : call to undeclared function 'signal'
```

### Which job failed
```bash
gh run view <RUN_ID> 2>&1 | grep -E "^[✓X\*] "
```

### FAILED project (Windows MSBuild)
```bash
gh run view <RUN_ID> --log-failed 2>&1 \
  | grep "build-windows" | grep "FAILED\." | head -5
```

---

## Step 4 — Diagnose by error category

### Missing include (`undeclared identifier`, `call to undeclared function`)
- The file uses a POSIX/stdlib symbol without including its header.
- Common gaps on MSVC clang-cl: `<signal.h>` (signal/SIGSEGV), `<strings.h>` (strcasecmp).
- Fix: add `#include <header.h>` at the top of the offending `.c` file.
- `qstring.h` handles `strtok_r` → `strtok_s` on MSVC; make sure the file includes `shared/qstring.h`.

### API mismatch (`has no member named 'X'`, `no matching function`)
- ObjectivelyMVC or Objectively source on GitHub is stale.
- Check local fix vs. remote: `git log origin/main..HEAD --oneline` in the dep repo.
- If local is ahead: push to `jdolan/` remote and re-trigger CI.

### Linker errors (`LNK2019`, `undefined reference`)
- New `.c` file added but not registered in all three build systems.
- Autotools: `src/<module>/Makefile.am`
- Xcode: `Quetoo.xcodeproj/project.pbxproj` (UUIDs must be 24 uppercase hex chars)
- Windows: `Quetoo.vs15/<lib>.vcxproj` + both `.sln` files

### `pointer being freed was not allocated` / crash handler loop
- Allocation/free mismatch. In crash-handler code (`Sys_Raise`, `Sys_Backtrace`):
  use `malloc`/`free`, **never** `Mem_Malloc`/`Mem_Free`.

---

## Step 5 — Fix, commit, push

```bash
# Edit the file(s), then:
make -j16 2>&1 | grep "error:" | head -10   # quick local check
make check                                   # run tests

git add <files>
git commit -m "<module>: fix <description>

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"

git push origin <branch>
```

---

## Step 6 — Confirm CI picks up the fix

```bash
sleep 15 && gh run list --branch $(git rev-parse --abbrev-ref HEAD) --limit 3
```

A new `in_progress` run should appear. Watch it complete:

```bash
gh run watch <NEW_RUN_ID>
```

---

## Noise-filter reference

The Windows build emits many `-Wc++-keyword` warnings for `this` identifiers
in Objectively/MVC C source. These are harmless warnings, not errors.

---

## Common one-liner: "show me all real errors from the latest run"

```bash
RUN=$(gh run list --branch $(git rev-parse --abbrev-ref HEAD) --limit 1 --json databaseId -q '.[0].databaseId') && \
gh run view $RUN --log-failed 2>&1 | python3 -c "
import sys, re
for line in sys.stdin:
    # Linux: file.c:line:col: error:
    # Windows: file.c(line,col): error :
    if re.search(r':[0-9]+:[0-9]+: error:|[^)]\([0-9]+,[0-9]+\): error :', line):
        # trim the log prefix (timestamp etc.) — last ~200 chars
        print(line.rstrip()[-220:])
" | sort -u | head -40
```

---

## Iteration checklist

- [ ] Latest run ID noted
- [ ] All errors extracted and categorized by platform
- [ ] Root cause identified (missing include / stale dep / linker / allocator)
- [ ] Fix applied locally, build clean, tests pass
- [ ] Dep repos pushed to `jdolan/` remote if needed
- [ ] Committed and pushed to PR branch
- [ ] New CI run triggered and confirmed green
