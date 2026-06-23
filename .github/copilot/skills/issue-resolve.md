# Skill: Resolve Issue (issue-resolve)

Use this skill whenever the user brings a GitHub issue to work on. It covers the
full lifecycle from triage through a merged, green-CI pull request.

---

## Step 1 — Triage the issue

```bash
# Read the full issue including all comments
gh issue view <ISSUE_NUMBER> --comments
```

Identify:
- **Type**: bug / feature / regression / performance / docs
- **Scope**: which subsystem(s) are affected (renderer `r_`, cgame `cg_`, game `g_`,
  server `sv_`, collision `cm_`, shared, quemap, etc.)
- **Reproduction**: is there a repro case, crash dump, or screenshot?
- **Acceptance**: what does "done" look like?

If the issue contains a Windows `.dmp` crash file, invoke the **`win-crash-debug`**
skill before proceeding.

---

## Step 2 — Explore the relevant code

Use the subsystem prefix to narrow the search:

```bash
# Find all files in a subsystem
find src/<subsystem>/ -name "*.c" -o -name "*.h" | head -20

# Search for a symbol or concept
grep -r "<keyword>" src/ --include="*.c" --include="*.h" -l

# Read the subsystem overview doc
cat .github/subsystems/<subsystem>.md
```

Read any relevant docs in `doc/copilot/` before touching renderer, shadow, or
entity-state code — those areas have known traps documented there.

Checklist before proceeding:
- [ ] Root cause identified (or hypothesis formed)
- [ ] All files that need to change are listed
- [ ] Issue updated with the appropriate labels

---

## Step 3 — Plan and get approval

For changes touching ≥ 2 files or adding/removing source files, write out the
plan **and wait for user approval** before editing anything:

```
Proposed changes:
1. src/client/renderer/r_foo.c — add X, fix Y
2. src/client/renderer/shaders/bsp_fs.glsl — update Z
3. src/client/renderer/Makefile.am — register new file (if any)

New files (need all 3 build systems updated):
- Makefile.am
- Quetoo.xcodeproj/project.pbxproj  (24-char uppercase hex UUIDs)
- Quetoo.vs15/<lib>.vcxproj + both .sln files
```

For small / obvious fixes (single-file typo, off-by-one, missing include) skip
the approval step and proceed directly to Step 4.

---

## Step 4 — Implement

Follow these conventions:

| Convention | Rule |
|------------|------|
| Types | `snake_case` with `_t` suffix (`vec3_t`, `entity_state_t`) |
| Functions/vars | `Xyz_TitleCase` with subsystem prefix |
| Memory | `Mem_Malloc` / `Mem_Free`; use plain `malloc`/`free` only for function-scope allocations |
| Vector math | `VectorCopy` / `VectorAdd` macros or `Vec3_*` functions |
| CVars | `cgi.AddCvar("r_name", "default", CVAR_ARCHIVE, "Description")` |
| OpenGL | OpenGL 4.1 Core Profile (macOS ceiling) |
| Comments | Only when clarification is needed; avoid obvious comments |
| Tests | For unit-testable changes, add or create tests in src/test using Check. Follow existing patterns and add .gitignore entries for test executables |

If adding/removing `.c` or `.h` files, update **all three** build systems:
1. `Makefile.am` in the affected `src/` subdirectory
2. `Quetoo.xcodeproj/project.pbxproj` — validate with `plutil -lint`
3. `Quetoo.vs15/<lib>.vcxproj` + `quetoo.sln` + `quetoo_all.sln`
Source file references sorted alphabetically in the project build files.

---

## Step 5 — Build and test

```bash
# Quick build check (from repo root)
make -j$(sysctl -n hw.logicalcpu) 2>&1 | grep "error:" | head -20

# Full test suite
make check

# Validate Xcode project if .pbxproj was touched
plutil -lint Quetoo.xcodeproj/project.pbxproj
```

Fix any errors before committing.

---

## Step 6 — Commit and open a PR

```bash
git add <files>
git commit -m "<subsystem>: <concise imperative description>

Fixes #<ISSUE_NUMBER>.

<Optional: 1–3 sentences on why, not what.>

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

Then use the `create_pull_request` tool (not `gh pr create`) so the PR card
appears in the UI. PR body template:

```
## Summary
<One paragraph describing the change.>

## Testing
- [ ] Build passes on macOS
- [ ] `make check` passes
- [ ] <Any manual verification steps>

Fixes #<ISSUE_NUMBER>
```

**Never push automatically.** Stop after `git commit` and wait for explicit
user approval before running `git push`.

---

## Step 7 — Watch CI

After the user approves the push, invoke the **`ci-watch-fix`** skill to
monitor CI and fix any failures.

---

## Iteration checklist

- [ ] Issue read + type/scope/acceptance identified
- [ ] Relevant code located, docs in `doc/copilot/` checked
- [ ] Plan approved (if ≥ 3 files or new files)
- [ ] Build systems updated if files added/removed
- [ ] `make -j<N>` clean
- [ ] `make check` passes
- [ ] `plutil -lint` passes (if `.pbxproj` touched)
- [ ] Committed with `Fixes #<N>` and Co-authored-by trailer
- [ ] PR opened via `create_pull_request` tool
- [ ] Push approved by user
- [ ] CI green (via `ci-watch-fix` skill)
