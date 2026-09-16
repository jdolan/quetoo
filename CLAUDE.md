# Quetoo

## User interface work (ObjectivelyMVC)

Anything under `src/cgame/common/ui/` or `src/client/ui/` is ObjectivelyMVC. Before touching a
View, a ViewController, a `.json` layout or a `.css` stylesheet, **read
`../ObjectivelyMVC/README.md`**, in particular its "Layout and styling are CSS-driven" section.

The rules that matter most, because violating them fails silently:

- **Layout and appearance are CSS.** Views are styled by the Selector / Style / Stylesheet system,
  never by assigning fields in C.
- **Never assign a `@styled` attribute in C.** `alignment`, `autoresizingMask`, `padding`,
  `width` / `height`, `minSize`, `pointerEvents`, `StackView::axis`, `StackView::spacing` and
  friends are all bound from the computed Style by `View::applyStyle` on every theme pass, so a C
  assignment is overwritten moments later. Check the header: if the attribute is marked `@styled`,
  it belongs in a stylesheet.
- **C is for structure and behaviour only:** creating and adding subviews, delegates, data source
  callbacks, and model values such as `Slider::min` / `max` / `value`.
- **Target views from CSS** by giving them a class (`$(view, addClassName, "foo")`) or an
  identifier, then writing the rule in the appropriate stylesheet: `ui/common.css` for anything
  shared across the cgame UI, the view controller's own `.css` for one screen, `ui/hud/<hud>/hud.css`
  for a specific HUD.
- **ObjectivelyMVC's own `Assets/stylesheet.css` is always in effect.** Its defaults include
  `StackView { axis: vertical }`, `Button { min-width: 100; padding: 8 8 8 8 }` and
  `Control { min-height: 32 }`. Compact icon buttons must overrule `min-width` *and* `min-height`;
  setting `width` alone will not shrink them.

When a view does not appear where or at the size you expect, check what the stylesheets are
already saying about it before changing any code.

## Building

`make -j8` from the repo root builds everything via autotools. Quetoo also ships Xcode
(`Quetoo.xcodeproj`, verified with `xcodebuild -workspace Quetoo.xcworkspace -scheme <target>`;
the `quetoo-all` scheme has a pre-existing SDL framework collision unrelated to any change) and
MSVC (`Quetoo.vs15/`) projects. **New source files must be added to all three**: the module's
`Makefile.am`, `Quetoo.xcodeproj/project.pbxproj`, and both `Quetoo.vs15/cgame_common.props` (the
list MSBuild actually compiles) and the per-target `.vcxproj.filters`.

## Assets

Engine chrome that renders before a game module loads (backgrounds, fonts, conback, loading,
progress bar, menu sounds) lives in this repo. In-game content, including HUD and UI icons under
`pics/`, lives in the sibling `quetoo-data` repo, under `target/default/` as the shared base that
every game module falls back to.
