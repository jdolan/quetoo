# Quetoo

**Read [`AGENTS.md`](../AGENTS.md) first.** It is the shared instruction set for coding agents, and
it carries the naming conventions, the build rules, the constraints that live outside this
repository, and the ordering traps.

Repeated here because it fails silently, with no compile error and no warning:

- Anything under `src/cgame/common/ui/` or `src/client/ui/` is ObjectivelyMVC, where **layout and
  appearance are CSS**. You MUST NOT assign a `@styled` attribute in C — `alignment`,
  `autoresizingMask`, `padding`, `width` / `height`, `minSize`, `pointerEvents`, `StackView::axis`
  and friends are rebound from the computed Style on every theme pass, so a C assignment is
  overwritten moments later. Check the header, then write a stylesheet rule. Before touching a View,
  a `.json` layout or a `.css` file, read `../ObjectivelyMVC/README.md`.
- An outlet identifier is a contract. `MakeOutlet("serverHostname", &this->hostnameLabel)` must
  match the `"identifier"` in the `.json` layout. Rename one, rename the other.
- A new source file MUST be added to all three build systems: `Makefile.am`,
  `Quetoo.xcodeproj/project.pbxproj`, and both `Quetoo.vs15/cgame_common.props` and the per-target
  `.vcxproj.filters`.
