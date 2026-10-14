---
name: bot-tuning
description: Investigate and tune Quetoo bot navigation and behavior on a map with the game's bot probe (g_aiProbe) and headless timeDemo matches driven by src/tools/ai_probe.py. Use when bots die, get stuck, get thrown by jump pads, ignore an item, or take a bad route on a map.
---

# Investigate and tune bots

Use this skill when a bot does something wrong on a map: it falls into the void, it is stuck, a
jump pad throws it, it never takes an item, or it takes a dangerous route. The method is to measure,
change one thing, and measure again over the same amount of game time.

The parts are:

| Part | What it does |
|---|---|
| `g_aiProbe` (`src/game/common/g_ai_probe.c`) | A developer cvar. The game writes what each bot does to `ai_probe_<map>.log` in the write directory. 1 records the bots, 2 the navigation graph, 4 the human players. The flags add: 5 records the bots and the players. The default, 0, records nothing. |
| `src/tools/ai_probe.py run` | Runs a headless dedicated server on the tree's build, at `timeDemo 1`, adds bots by rcon, and stops the server cleanly. |
| `src/tools/ai_probe.py summary`, `deaths`, `pushes`, `hazards` | Count and explain deaths, unplanned pushes, entries into lava or slime, pickups and stuck time. |
| `src/tools/ai_probe.py node`, `near`, `path`, `routes` | Query the navigation graph that the game used. |
| `cg_drawAiNodes`, `aiNodeInfo` (`src/cgame/common/cg_nav.c`) | Draw the graph in the client, and print the node nearest the view. |

## 1. Build

```sh
make -j8
```

`ai_probe.py run` uses `src/main/quetoo-dedicated` and `src/game/<module>/.libs/game.so` from the
tree. It links them into a private stage, so it never needs `make install` and never touches the
user directory.

## 2. Run a match

```sh
python3 src/tools/ai_probe.py run longyard --seconds 60 --bot Bastard:0.8 --graph --out "$TMPDIR/ly1"
```

- `--bot NAME[:SKILL[,AGGRESSION[,AWARENESS]]]` adds a roster bot (`g_ai_info.c`) with `addAi`.
  Repeat it for more bots. Traits MUST be from 0 to 1. A skill above 1 makes the strafe error
  grow without limit, and the results mean nothing.
- `--clients N` sets `sv_minClients`, and the game fills the server with random roster bots.
- `--graph` also records the graph, which the graph commands need. It adds about 250 KB.
- Arguments after the options go to the server, for example `+set g_gravity 400`.
- Use a different `--port` for each match that runs at the same time.

`timeDemo 1` runs the server as fast as it can. One bot on longyard plays about 170 game seconds
in each wall second. More bots run slower.

The tool prints the log path and a summary. The log is in `<out>/write/`, and the server's console
output is in `<out>/server.log`.

### Why the tool works the way it does

These facts are not visible from the call sites. You MUST NOT "simplify" the tool around them.

- **A dedicated server ignores standard input when it is not a TTY** (`Sv_InitConsole`). You
  cannot send it `quit`.
- **SIGTERM shuts the server down cleanly**, and the game then closes the probe log. SIGKILL loses
  the log, because the file is buffered.
- **Commands after `+map` do not run on a dedicated server**, even after `+wait`: the server holds
  them until a client begins (`Cbuf_CopyToDefer`). So the tool adds bots by rcon.
- **Packets queue while the map loads**, and the server runs all of them after the load. The tool
  repeats only a harmless `echo` until the server answers, and then sends each `addAi` once. A
  repeated `addAi` adds the bot more than once.
- **`rconPassword` is an archived cvar.** The tool sets it on the command line, so the server
  MUST write its config to the private write directory (`-w`), never to the user directory.
- **`sv_public 0`** keeps the match off the master server.

## 3. Read the result

```sh
python3 src/tools/ai_probe.py summary "$LOG"
python3 src/tools/ai_probe.py deaths "$LOG" --link 616-636 --count 3
python3 src/tools/ai_probe.py deaths "$LOG" --near 0,800,300
python3 src/tools/ai_probe.py pushes "$LOG" --link 309-308
python3 src/tools/ai_probe.py hazards "$LOG" --near 250,-500,250
```

- `summary` groups deaths and unplanned pushes by the link that the bot last followed. Start with
  the largest group.
- `deaths`, `pushes` and `hazards` print each event with the 32 samples before it, 100 ms apart:
  position, velocity, link, link type, strafe jump state, ground, the bot's state, and whether
  damage knocked it back. Read the samples to see what the bot
  did: whether it strafe jumped, overshot its node, or left its link.
- A death below about z -900 on a map with a void is a fall. The position of the death is where
  the bot hit the hurt trigger, often far from the edge that it fell from. The samples show the
  edge.
- An unplanned push is a push from a `trigger_push` while the bot does not follow a jump pad link,
  and its next link is not one either. `summary` and `pushes` read only those. The knockback of damage also sets `PMF_TIME_PUSHED`, so the
  probe records a push only within 32 units of a `trigger_push`.

### The log format

Each line is `PROBE <time> <client> <EVENT> <key>=<value> ...`. The time is `gameLevel.time` in
milliseconds. The client is -1 for an event of the level. A value MUST NOT contain a space.

| Event | Written |
|---|---|
| `GRAPH` | Once, with the node and link counts |
| `NODE`, `TRIGGER` | Once, with `g_aiProbe 2`: each node, `edge=1` if it stands at the edge of the void, with its links, as `node:type:cost`, or `node:type:cost:door` for a walk through the doorway of the door with inline model `door`, and each push and hurt trigger's bounds |
| `CLIMB` | Once, with `g_aiProbe 2`, after the `NODE` of its bottom: each ladder climb, with its bottom and top nodes, the `column` where the climb meets the ladder (the foot, or where a jump grabs it), the `landing` over the top, and the time in hundredths of a second |
| `POS` | Each second for each bot: position, speed, state, goal item, health, link, the weapon in hand, the weapons it can fire, whether it counts itself armed, the enemy's distance, the combat movement, the water level (0 to 3) and whether it is on the ground |
| `SPEED` | Each 5 seconds: roster name, skill, average and highest speed, the share of time that the bot strafe jumps and is in the air, and the share of time by the reason that it does or does not strafe jump on a path (`GameAiStrafeBlock`): `goal` (not on a path), `strafing`, `lowskill`, `state`, `turn`, `short`, `unsafe`, `fall`. These eight sum to 1 |
| `DEATH` + `HIST reason=death` | When a bot dies, from any cause |
| `HAZARD` + `HIST reason=hazard` | When a bot enters lava or slime: its link, state, whether it has an enemy, and `lava=1` for lava |
| `KILL` | With flag 1 or 4, when any bot or player dies: the attacker (the victim's own number for a suicide, -1 for the world), the means of death (`mod`, as `GameMeansOfDeath`), the weapon in the attacker's hand, the range, and whether each is a bot |
| `VISIT` | Each time a client moves to another node: the client's route, node by node. The client leaves a node only for one at least 16 units nearer. A death starts a new route. Bots with flag 1, players with flag 4 |
| `CMD` | With flag 4, each movement command of a human player: the nearest node (-1 if none is within 128 units), position, velocity, view angles (yaw, pitch), the move (forward, right, up), its duration, the ground, the water level and the buttons. Up to 250 lines a second for each player, about 150 MB an hour |
| `PATH` | When a bot gets a new path: where it is, its speed, whether it is on the ground, and the first, second and last nodes |
| `PATHEND` | When the game ends a bot's path, and why: `arrived`, `distress`, `unlinked` or `teleported` |
| `PUSHED` | When a push trigger throws a bot. `planned=0` if the bot does not follow a pad link, with `HIST reason=pushed` |
| `JUMP`, `LAND` | When a bot takes off on a gap jump link, and where it lands |
| `PICKUP` | When a bot, or with flag 4 a player, takes an item |
| `GATE` | Once, for each gate: the `targetname` of doors that a button moves, with links through them, and the counts of its buttons, of the nodes from which a bot presses them, and of its doors |
| `GATEUSE` | When a bot chooses a gate on the way to an item (`choose`), goes through it once the doors move (`open`), or gives up (`fail`). `summary` counts them for each gate |
| `STUCK` | Each second that a bot on a path, out of combat, has stayed within 96 units over its last 6 `POS` events, which span 5 seconds, with `HIST reason=stuck` at the first |

Link types are the `NavTravelType` values: 0 for the links that `G_Ai_Node_Link` adds to splice
item and spawn nodes into the graph, 1 walk, 2 crouch, 3 walk off ledge, 4 barrier jump,
5 teleport, 6 push, 7 plat, 8 swim, 9 jump, 10 ladder.

## 4. Query the graph

```sh
python3 src/tools/ai_probe.py node "$LOG" 636           # its links, and the links into it
python3 src/tools/ai_probe.py near "$LOG" 0 448 588      # the nodes near a point
python3 src/tools/ai_probe.py path "$LOG" 87 8           # the cheapest path, link by link
python3 src/tools/ai_probe.py routes "$LOG" 8 669 670    # how many routes to node 8 cross 669 or 670
```

`path` and `routes` approximate `G_Ai_Node_StepCost`. They do not know about crushers, raised
platforms, hazards or movers. Use them to see which route the planner prefers, and why.

Node ids belong to one build of one map. If you change `bg_nav.c`, record the graph again.

## 5. Learn from a player

Jay plays against the bots, or alone, with the probe on. He sets it before the map loads, for
example `+set g_aiProbe 5`, and the log is in his write directory. Then:

```sh
python3 src/tools/ai_probe.py route "$LOG" --client 0         # a client's route and how it covers the map
python3 src/tools/ai_probe.py linktimes "$LOG"                 # the node pairs that bots cross most slowly
python3 src/tools/ai_probe.py moves "$LOG" --link 1458-1459    # the player's commands over a link
python3 src/tools/ai_probe.py strafe "$LOG"                    # the player's jumps by takeoff speed
python3 src/tools/ai_probe.py kills "$LOG"                     # kills by attacker, weapon and range
```

- `linktimes` compares the time between two consecutive `VISIT` nodes, for players and bots. A
  pair that a bot crosses much more slowly is a place to look at the bot's movement.
- A pair of consecutive `VISIT` nodes with no link between them in the graph is a move that the
  graph does not have, such as a trick jump.
- One player's style is a reference, not a target: scale toward it by skill.
- Node ids belong to one build of the graph. `VISIT` and `CMD` also hold positions. To compare
  recordings across graph builds, use the positions, not the node ids. The tool does not map them
  yet.

## 6. Watch it in the client

To see what a player sees, run the client from the same stage, with the graph drawn:

```sh
./src/main/quetoo -p "$OUT/stage" -w "$TMPDIR/client" +set s_volume 0 +set cg_drawAiNodes 1 +map longyard
```

- MUST pass `+set s_volume 0`.
- `aiNodeInfo` prints the node nearest the view, with its links.
- The client draws the graph that `Nav_Build` makes. The game adds item, pad, teleporter and plat
  nodes after that, so ids at or above the build's node count appear only in the probe's graph.
- Node colors: white normal, red crouch. Link colors: green walk, red crouch, grey ledge, yellow
  barrier jump, orange gap jump, cyan swim, violet ladder.
- `screencapture` takes a screenshot of the window for a report.

## 7. Method

- **Measure before you change anything.** Record a baseline on the current build.
- **Compare equal game time.** Use the event times, not the count of `SPEED` lines, which come
  every 5 seconds. Run at least two matches for each build: bots make random choices.
- **Keep the old build to compare with.** For example, save the change with
  `git diff <files> > change.patch`, then `git apply -R change.patch`, `make`, run, and
  `git apply change.patch`. Copy the changed files aside first. MUST NOT `git checkout` a file
  with uncommitted work to undo a temporary edit: if a step before it fails, the work is lost.
  Check that the patch is not empty, and in zsh write the file list out, because zsh does not
  split a variable into words.
- **Read the whole death table after a fix.** A fix that opens a route sends bots to new places,
  and a new group of deaths can appear somewhere else.
- **Look for a new path in the air.** A bot that falls off an edge often got a new path just
  before, while it was in the air. Read the `PATH` and `PATHEND` events of the seconds before a
  death: where the new path starts, and whether the bot was on the ground.
- **Separate planning from steering.** If the planner chooses a dangerous route, look at the graph
  and the costs (`path`, `routes`). If the route is fine but the bot leaves it, look at the
  samples (`deaths`, `pushes`) and at the movement code in `g_ai_main.c`.
- **Record what you find in the commit body**, with the map, the bot, the skill, the game time and
  the numbers before and after. That is where Quetoo keeps the reasons for its design.

## 8. Add an event

Add it to `g_ai_probe.c`, with a `G_Ai_Probe_*` hook where the game knows about it, and read it in
`ai_probe.py`. Keep the line format: `key=value` pairs, no spaces in a value, positions as
`x,y,z`. MUST NOT leave temporary probe code anywhere else in the game: if a question needs new
data, add the event to the module so that the next investigation has it.

## 9. Before you commit

- `g_aiProbe` MUST stay 0 by default.
- `git diff` MUST show no temporary instrumentation outside `g_ai_probe.c`.
- A read-only subagent reviews the change, as for any change.
- A new source file goes into all three build systems. `python3 src/tools/verify_projects.py`
  checks that they agree.
