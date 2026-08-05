#!/usr/bin/env python3
"""Verify that every module's sources agree across autotools, MSVS and Xcode.

Each game and client game module is described three times - in its Makefile.am,
in the MSVS project and property sheet, and in the Xcode project - and only
autotools is exercised by a local build on macOS or Linux. A module that is
missing a source, that compiles another module's manifest, or that is built with
a different set of feature defines than the objects it links, produces no build
failure in the system you are looking at. It presents as a network fault, a menu
offering the wrong game modes, or nothing at all until someone builds on Windows.

This is the "compare each target's Sources against its Makefile.am, in every
build system" check that doc/game-module-hooks.md prescribes.
"""

import argparse
import collections
import os
import re
from pathlib import Path

# Autotools resolves these through vpath, so a module's own copy shadows common's.
# Whichever project file names the wrong directory silently builds the wrong one.
PER_MODULE_FILES = (
  "bg_item.c",
  "bg_item.h",
  "cg_module.c",
  "cg_team_mode.c",
  "g_module.c",
  "g_types.h",
)

# Windows has no separate discord-rpc project, so each cgame module compiles it
# in; autotools links deps/discord-rpc/libdiscord-rpc.la instead.
MSVS_ONLY = {
  "connection_win.cpp",
  "discord_register_win.cpp",
  "discord_rpc.cpp",
  "dllmain.cpp",
  "rpc_connection.cpp",
  "serialization.cpp",
}

MODULES = ("default", "ctf", "lithium")

# The MSVS project is named for the module except for default, which predates
# the others and is simply "game" / "cgame".
def msvs_project(kind: str, module: str) -> str:
  return f"{kind}.vcxproj" if module == "default" else f"{kind}-{module}.vcxproj"


class Autotools:
  """A module's Makefile.am."""

  def __init__(self, root: Path, kind: str, module: str):
    self.path = root / "src" / kind / module / "Makefile.am"
    self.text = self.path.read_text()

  def sources(self) -> set:
    var = "cgame_la_SOURCES" if "cgame" in str(self.path) else "game_la_SOURCES"
    m = re.search(rf'^{var}\s*=\s*((?:.*\\\n)*.*)$', self.text, re.M)
    if not m:
      return set()
    body = m.group(1).replace("\\\n", " ")
    return {os.path.basename(t) for t in body.split() if t.endswith((".c", ".cpp"))}

  def defines(self) -> set:
    return set(re.findall(r'-D(G_[A-Z]+)', self.text))


class Msvs:
  """A module's .vcxproj, plus the property sheet it imports."""

  def __init__(self, root: Path, kind: str, module: str):
    self.dir = root / "Quetoo.vs15"
    self.project = (self.dir / msvs_project(kind, module)).read_text()
    self.props = (self.dir / f"{kind}_common.props").read_text()
    # An opt-in feature's sources sit in a Condition'd group in the property
    # sheet, switched on by a property the module's own project sets.
    self.enabled = set(re.findall(r'<(Quetoo\w+)>true</\1>', self.project))

  def _items(self, tag: str) -> set:
    out = set()
    for text in (self.project, self.props):
      for group in re.finditer(r'<ItemGroup([^>]*)>(.*?)</ItemGroup>', text, re.S):
        condition = re.search(r"Condition=\"'\$\((\w+)\)'=='true'\"", group.group(1))
        if condition and condition.group(1) not in self.enabled:
          continue
        for include in re.findall(rf'<{tag} Include="([^"]+)"', group.group(2)):
          out.add(include.replace("\\", "/").removeprefix("../"))
    return out

  def sources(self) -> set:
    return self._items("ClCompile")

  def headers(self) -> set:
    return self._items("ClInclude")

  def defines(self) -> set:
    m = re.search(r'<PreprocessorDefinitions>([^<]*)</PreprocessorDefinitions>', self.project)
    return {d for d in (m.group(1).split(";") if m else []) if d.startswith("G_")}


class Xcode:
  """The one project file, indexed by target.

  This parses the pbxproj with regexes over its indentation, because the format
  is a plist that Xcode writes in exactly one shape. That is fragile by nature,
  so every accessor asserts it found something: a parse that silently returns
  nothing would otherwise read as two build systems agreeing.
  """

  def __init__(self, root: Path):
    self.text = (root / "Quetoo.xcodeproj" / "project.pbxproj").read_text()

    self.files = {}
    for m in re.finditer(r'\t\t(\w+) /\* [^*]* \*/ = \{isa = PBXFileReference;([^}]*)\};', self.text):
      path = re.search(r'path = "?([^";]+)"?;', m.group(2))
      if path:
        self.files[m.group(1)] = path.group(1)

    self.groups, self.parents = {}, {}
    for m in re.finditer(r'\t\t(\w+) /\* [^*]* \*/ = \{\n\t\t\tisa = PBXGroup;\n(.*?)\n\t\t\};',
                         self.text, re.S):
      body = m.group(2)
      path = re.search(r'path = "?([^";]+)"?;', body)
      children = re.findall(r'\t\t\t\t(\w+) /\*', body)
      self.groups[m.group(1)] = path.group(1) if path else None
      for child in children:
        self.parents[child] = m.group(1)

    self.build_files = dict(re.findall(
      r'\t\t(\w+) /\* [^*]* \*/ = \{isa = PBXBuildFile; fileRef = (\w+) /\*', self.text))

    self.phases = {}
    for kind in ("Sources", "Headers"):
      for m in re.finditer(
          rf'\t\t(\w+) /\* {kind} \*/ = \{{\n\t\t\tisa = PBX{kind}BuildPhase;\n(.*?)\n\t\t\}}',
          self.text, re.S):
        self.phases[m.group(1)] = (kind, re.findall(r'\t\t\t\t(\w+) /\*', m.group(2)))

    self.targets = {}
    for m in re.finditer(
        r'\t\t(\w+) /\* ([^*]+) \*/ = \{\n\t\t\tisa = PBXNativeTarget;\n(.*?)\n\t\t\};\n',
        self.text, re.S):
      self.targets[m.group(2)] = m.group(3)

    assert self.files, "parsed no file references; the pbxproj format may have changed"
    assert self.targets, "parsed no targets; the pbxproj format may have changed"

  def _path(self, file_id: str) -> str:
    parts = [self.files.get(file_id, "?")]
    parent = self.parents.get(file_id)
    while parent:
      if self.groups.get(parent):
        parts.insert(0, self.groups[parent])
      parent = self.parents.get(parent)
    return "/".join(parts)

  def _phase(self, target: str, kind: str) -> set:
    body = self.targets[target]
    out = set()
    for phase_id in re.findall(r'\t\t\t\t(\w+) /\* \w+ \*/,', body):
      if phase_id in self.phases and self.phases[phase_id][0] == kind:
        for build_file in self.phases[phase_id][1]:
          if build_file in self.build_files:
            out.add(self._path(self.build_files[build_file]))
    return out

  def sources(self, target: str) -> set:
    out = self._phase(target, "Sources")
    assert len(out) > 20, f"{target}: parsed only {len(out)} sources, expected dozens"
    return out

  def headers(self, target: str) -> set:
    return self._phase(target, "Headers")

  def defines(self, target: str) -> set:
    configs = re.search(
      rf'\t\t\w+ /\* Build configuration list for PBXNativeTarget "{re.escape(target)}" \*/ = \{{(.*?)\n\t\t\}};',
      self.text, re.S)
    assert configs, f"{target}: found no build configuration list"
    out = set()
    for config_id in re.findall(r'\t\t\t\t(\w+) /\*', configs.group(1)):
      config = re.search(
        rf'\t\t{config_id} /\* \w+ \*/ = \{{\n\t\t\tisa = XCBuildConfiguration;(.*?)\n\t\t\}};',
        self.text, re.S)
      if config:
        out |= set(re.findall(r'"?(G_[A-Z]+)"?', config.group(1)))
    return out


def verify_module(root: Path, kind: str, module: str, xcode: Xcode, verbose: bool) -> list:
  """Compare one module's three descriptions. Returns a list of complaints."""

  target = f"{kind}-{module}"
  am = Autotools(root, kind, module)
  msvs = Msvs(root, kind, module)

  expected = am.sources()
  found = {
    "msvs": collections.Counter(os.path.basename(p) for p in msvs.sources()),
    "xcode": collections.Counter(os.path.basename(p) for p in xcode.sources(target)),
  }

  problems = []
  for name, counted in found.items():
    for missing in sorted(expected - set(counted)):
      problems.append(f"{target}: {name} is missing {missing}")
    for extra in sorted(set(counted) - expected - MSVS_ONLY):
      problems.append(f"{target}: {name} builds {extra}, which its Makefile.am does not list")
    for basename, count in sorted(counted.items()):
      if count > 1:
        problems.append(f"{target}: {name} lists {basename} {count} times")

  # A module must build its own manifest, not a sibling's. Autotools gets this
  # from vpath; the project files have to name the right directory.
  for path in sorted(msvs.sources() | xcode.sources(target) |
                     msvs.headers() | xcode.headers(target)):
    if os.path.basename(path) in PER_MODULE_FILES:
      if not os.path.dirname(path).endswith("/" + module):
        problems.append(f"{target}: builds {path}, expected the copy under .../{module}")

  defines = {"autotools": am.defines(), "msvs": msvs.defines(), "xcode": xcode.defines(target)}
  if len(set(map(frozenset, defines.values()))) != 1:
    problems.append(f"{target}: feature defines differ: " +
                    ", ".join(f"{k}={sorted(v) or 'none'}" for k, v in defines.items()))

  if verbose:
    print(f"  {target}: {len(expected)} sources, defines {sorted(defines['autotools']) or 'none'}")

  return problems


def verify_common_files(root: Path, kind: str, xcode: Xcode) -> list:
  """Every file in common must be known to MSVS and Xcode.

  Autotools needs no such check: a source it does not list is simply not built,
  which the module comparison above already catches. This catches the file that
  was added to every Makefile.am and forgotten in the project files.
  """

  msvs = Msvs(root, kind, "ctf")  # ctf enables every feature, so nothing is filtered out
  listed = {os.path.basename(p) for p in msvs.sources() | msvs.headers()}
  referenced = set(xcode.files.values())

  problems = []
  for path in sorted((root / "src" / kind / "common").glob("*.[ch]")):
    if path.name not in listed:
      problems.append(f"{kind}/common/{path.name} is in no MSVS project or property sheet")
    if path.name not in referenced:
      problems.append(f"{kind}/common/{path.name} has no Xcode file reference")
  return problems


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__,
                                   formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2],
                      help="Quetoo checkout to verify (default: the one holding this script)")
  parser.add_argument("--verbose", action="store_true", help="Print each module as it is checked")
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  root = args.root.expanduser().resolve()

  xcode = Xcode(root)

  problems = []
  for kind in ("game", "cgame"):
    if args.verbose:
      print(f"{kind}:")
    for module in MODULES:
      problems += verify_module(root, kind, module, xcode, args.verbose)
    problems += verify_common_files(root, kind, xcode)

  if problems:
    for problem in problems:
      print(f"error: {problem}")
    print(f"\n{len(problems)} problem(s) found")
    return 1

  print(f"{len(MODULES) * 2} module(s) agree across autotools, MSVS and Xcode")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
