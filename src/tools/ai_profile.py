#!/usr/bin/env python3
"""
ai_profile.py - run headless bot matches and read the game's bot profiler

The game module records what bots do when the developer cvar `g_aiProfile` is set (see
src/game/common/g_ai_profile.h): 1 records the bots, 2 the navigation graph, 3 both. It writes
`ai_profile_<map>.log` to the write directory. 4 records human players: each movement command
(CMD, with the view angles as yaw,pitch), each node passed (VISIT) and each pickup; 5 records the
bots and the players. Each line is

    PROFILE <time> <client> <EVENT> <key>=<value> ...

Usage:
    ai_profile.py run <map> [--seconds 60] [--bot Bastard:0.8] [--clients N] [--graph] [--out DIR] [+set ...]
    ai_profile.py summary <log> [<log> ...]
    ai_profile.py deaths <log> [--link A-B] [--near X,Y,R] [--count N]
    ai_profile.py pushes <log> [--link A-B] [--count N]
    ai_profile.py hazards <log> [--link A-B] [--near X,Y,R] [--count N]   (entries into lava or slime)
    ai_profile.py node <log> <id> [<id> ...]
    ai_profile.py near <log> X Y Z [--radius R]
    ai_profile.py path <log> <from> <to>
    ai_profile.py routes <log> <goal> <node> [<node> ...]
    ai_profile.py route <log> [--client N] [--top 10] [--show N]
    ai_profile.py moves <log> (--node N | --link A-B) [--client N] [--count 5] [--raw]
    ai_profile.py strafe <log> [--client N]
    ai_profile.py linktimes <log> [<log> ...] [--min 3] [--count 20]   (logs of one graph build)
    ai_profile.py kills <log> [--module default]
    ai_profile.py suite <out> [--seconds 40] [--bot NAME ...] [--maps M,M ...] [--jobs 7] [--graph]
                        [--tree DIR] [+set ...]
    ai_profile.py compare <base>[,<base> ...] <current>[,<current> ...]
    ai_profile.py graphdiff <base> <current>

`run` builds the tree's dedicated server and game module into a private stage, so it never touches
the user directory. `--tree` runs the build of another tree, for example a worktree of the base
commit, with this tree's tool. `suite` runs a match on each map of a set, several at a time.
`compare` reads two or more suites and prints the deaths, kills and pickups per game minute of each
map. `graphdiff` tells whether two suites recorded with `--graph` built the same graphs. Each suite
takes one port for each map, from `--port`. The `node`, `near`, `path` and `routes` commands need a
log that was recorded with `--graph`. Their path costs approximate `G_Ai_Node_StepCost`: the link
cost, plus the penalty for a drop over 128 units, the falling damage that a full-health bot takes,
and the penalty for a node at the edge of the void. The game also multiplies the cost of a link into
water by AI_WATER_EXPOSURE (1.5); the log does not say which nodes are in water, so these costs do
not.
"""

import argparse
import collections
import heapq
import math
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

TRAVEL = {-1: '-', 0: 'none', 1: 'walk', 2: 'crouch', 3: 'walk_off_ledge', 4: 'barrier_jump', 5: 'teleport',
          6: 'push', 7: 'plat', 8: 'swim', 9: 'jump', 10: 'ladder'}

RCON_PASSWORD = 'profile'

SUITE_MAPS = ['2deaths', 'abase', 'aerowalk', 'aghast', 'badplace', 'campgrounds', 'chthon', 'cistern',
              'claustrophobopolis', 'darkzone', 'edge', 'fraggin', 'fragpipe', 'gehenna', 'hallways',
              'lavatomb', 'longyard', 'miner', 'omf', 'pits', 'rage', 'rounding', 'slimyplace', 'tokays',
              'torn', 'warehouse']

SUITE_BOTS = ['Bastard:0.99', 'Guard', 'Stroggo']

GRAPH_EVENTS = ('GRAPH', 'NODE', 'TRIGGER', 'CLIMB')

LINE = re.compile(r'^PROFILE (\d+) (-?\d+) (\w+) ?(.*)$')


# ---------------------------------------------------------------------------
# Reading the log
# ---------------------------------------------------------------------------

class Event:
    """One line of the log."""

    def __init__(self, time, client, kind, fields):
        self.time = time
        self.client = client
        self.kind = kind
        self.fields = fields

    def __getitem__(self, key):
        return self.fields[key]

    def get(self, key, default=None):
        return self.fields.get(key, default)

    def pos(self, key='pos'):
        return tuple(int(v) for v in self.fields[key].split(','))

    def link(self):
        return self.fields.get('link', '-1->-1')

    def __str__(self):
        fields = ' '.join(f'{k}={v}' for k, v in self.fields.items())
        return f'{self.time:>8} {self.client:>3} {self.kind:<8} {fields}'


def read_log(path):
    events = []
    with open(path, errors='replace') as f:
        for line in f:
            m = LINE.match(line.rstrip('\n'))
            if not m:
                continue
            fields = dict(kv.split('=', 1) for kv in m.group(4).split() if '=' in kv)
            events.append(Event(int(m.group(1)), int(m.group(2)), m.group(3), fields))
    return events


def histories(events, reason):
    """Pair each DEATH, PUSHED or HAZARD event with the HIST samples that the game writes right after it."""
    kind = {'death': 'DEATH', 'pushed': 'PUSHED', 'hazard': 'HAZARD'}[reason]
    out = []
    for i, e in enumerate(events):
        if e.kind != kind or e.get('planned') == '1':
            continue
        samples = []
        for h in events[i + 1:]:
            if h.kind != 'HIST' or h.client != e.client or h.get('reason') != reason:
                break
            samples.append(h)
        out.append((e, samples))
    return out


def last_link(event, samples):
    """The link that the bot last followed before the event: its own, or that of its samples."""
    if event.link() != '-1->-1':
        return event.link(), TRAVEL.get(int(event.get('type', -1)), '?')
    for s in reversed(samples):
        if s.link() != '-1->-1':
            return s.link(), TRAVEL.get(int(s.get('type', -1)), '?')
    return 'no path', TRAVEL[-1]


def game_seconds(events):
    return max((e.time for e in events), default=0) / 1000.0


# ---------------------------------------------------------------------------
# Reading the graph
# ---------------------------------------------------------------------------

def read_graph(events):
    nodes = {}
    for e in events:
        if e.kind != 'NODE':
            continue
        links = []
        gates = {}
        if e['links']:
            for link in e['links'].split(','):
                to, travel, cost, *gate = (int(v) for v in link.split(':'))
                links.append((to, travel, cost))
                if gate:
                    gates[to] = gate[0]
        nodes[int(e['id'])] = {'pos': e.pos(), 'edge': e.get('edge') == '1', 'links': links, 'gates': gates}
    if not nodes:
        sys.exit('The log has no graph: record it with `run --graph`, or g_aiProfile 3')
    return nodes


def fall_damage(drop, gravity=800.0):
    if drop <= 0:
        return 0.0
    velocity = -math.sqrt(2.0 * gravity * drop)
    if velocity > -700.0:
        return 0.0
    return -((velocity + 700.0) * 0.05)


EDGE_PENALTY = 32.0


def step_cost(nodes, a, b, cost, health=100):
    drop = nodes[a]['pos'][2] - nodes[b]['pos'][2]
    if drop > 512:
        return None
    penalty = (drop - 128) * 0.5 if drop > 128 else 0.0
    damage = fall_damage(drop)
    if damage + 8 >= health:
        return None
    return cost + penalty + damage * 6 + (EDGE_PENALTY if nodes[b]['edge'] else 0.0)


def shortest_paths_to(nodes, goal):
    """Dijkstra over reversed links: the cost from every node to `goal`, and each node's next hop."""
    reverse = collections.defaultdict(list)
    for a, node in nodes.items():
        for b, travel, cost in node['links']:
            c = step_cost(nodes, a, b, cost)
            if c is not None and b in nodes:
                reverse[b].append((a, c))
    dist = {goal: 0.0}
    hop = {}
    queue = [(0.0, goal)]
    while queue:
        d, u = heapq.heappop(queue)
        if d > dist[u]:
            continue
        for v, c in reverse[u]:
            if d + c < dist.get(v, math.inf):
                dist[v] = d + c
                hop[v] = u
                heapq.heappush(queue, (d + c, v))
    return dist, hop


def walk(hop, start):
    path = [start]
    while path[-1] in hop:
        path.append(hop[path[-1]])
    return path


def link_type(nodes, a, b):
    for to, travel, cost in nodes[a]['links']:
        if to == b:
            return TRAVEL.get(travel, '?')
    return '?'


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_run(args):
    tree = Path(args.tree).resolve() if args.tree else ROOT
    dedicated = tree / 'src/main/quetoo-dedicated'
    game = tree / f'src/game/{args.module}/.libs/game.so'
    cgame = tree / f'src/cgame/{args.module}/.libs/cgame.so'

    for path in (dedicated, game):
        if not path.exists():
            sys.exit(f'{path} is missing: build the tree with `make -j8` first')

    out = Path(args.out or tempfile.mkdtemp(prefix=f'ai_profile_{args.map}_')).resolve()
    stage = out / 'stage' / args.module
    write = out / 'write'
    stage.mkdir(parents=True, exist_ok=True)
    write.mkdir(parents=True, exist_ok=True)

    for link, target in ((stage / 'game.so', game), (stage / 'cgame.so', cgame)):
        if link.is_symlink() or link.exists():
            link.unlink()
        if target.exists():
            link.symlink_to(target)

    log = write / f'ai_profile_{args.map}.log'
    if log.exists():
        log.unlink()

    command = [str(dedicated), '-p', str(out / 'stage'), '-w', str(write),
               '+set', 'sv_public', '0',
               '+set', 'net_port', str(args.port),
               '+set', 'rconPassword', RCON_PASSWORD,
               '+set', 'sv_minClients', str(args.clients),
               '+set', 'g_timeLimit', '0',
               '+set', 'g_fragLimit', '0',
               '+set', 'timeDemo', '1',
               '+set', 'g_aiProfile', '3' if args.graph else '1',
               *(['+game', args.module] if args.module != 'default' else []),
               *args.extra,
               '+map', args.map]

    with open(out / 'server.log', 'w') as server_log:
        server = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=server_log, stderr=subprocess.STDOUT,
                                  cwd=tree)
    try:
        wait_for_server(args.port, server)
        for bot in args.bot:
            add_bot(args.port, bot)
        time.sleep(args.seconds)
    finally:
        server.send_signal(signal.SIGTERM)
        try:
            server.wait(timeout=30)
        except subprocess.TimeoutExpired:
            server.kill()
            sys.exit(f'The server did not quit on SIGTERM; {log} may be incomplete')

    if not log.exists():
        sys.exit(f'No profile log: see {out / "server.log"}')

    print(log)
    summarize(log, read_log(log))


def rcon(sock, port, command):
    sock.sendto(b'\xff\xff\xff\xffrcon ' + f'{RCON_PASSWORD} {command}'.encode(), ('127.0.0.1', port))


def wait_for_server(port, server):
    """Send `echo` until the server answers. Packets queue while the map loads, so only a harmless
    command is sent more than once."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(0.5)
        deadline = time.time() + 120
        while time.time() < deadline:
            if server.poll() is not None:
                sys.exit(f'The server exited with {server.returncode} before it was ready')
            rcon(sock, port, 'echo ready')
            try:
                if 'ready' in sock.recv(8192).decode(errors='replace'):
                    return
            except socket.timeout:
                pass
        sys.exit('The server did not answer rcon in 120 seconds')


def add_bot(port, bot):
    name, _, traits = bot.partition(':')
    command = f'addAi {name} {" ".join(traits.split(","))}'.strip()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(10)
        rcon(sock, port, command)
        try:
            reply = sock.recv(8192).decode(errors='replace')
        except socket.timeout:
            sys.exit(f'`{command}` got no answer in 10 seconds')
    if 'Added' not in reply:
        sys.exit(f'`{command}` failed: {reply.strip()}')


def print_by_link(pairs, count=10):
    """Print the commonest (link, travel type) pairs, with their counts."""
    for (link, travel), n in collections.Counter(pairs).most_common(count):
        print(f'    {n:>5}  {link} {travel}')


def summarize(path, events):
    seconds = game_seconds(events)
    names = {}
    skills = {}
    speeds = collections.defaultdict(list)
    for e in events:
        if e.kind == 'SPEED':
            names[e.client] = e['name']
            skills[e.client] = e['skill']
            speeds[e.client].append(int(e['avg']))

    deaths = histories(events, 'death')
    pushes = histories(events, 'pushed')
    pickups = collections.Counter(e['item'] for e in events if e.kind == 'PICKUP')
    stuck = collections.Counter(e.link() for e in events if e.kind == 'STUCK')
    graph = next((e for e in events if e.kind == 'GRAPH'), None)

    print(f'{path}: {seconds:.0f} game seconds' + (f', {graph["nodes"]} nodes, {graph["links"]} links' if graph else ''))
    for client in sorted(names):
        avg = sum(speeds[client]) / len(speeds[client])
        print(f'  client {client}: {names[client]} skill {skills[client]}, average speed {avg:.0f}')

    per_minute = 60.0 / seconds if seconds else 0.0
    print(f'  deaths: {len(deaths)} ({len(deaths) * per_minute:.2f} a minute)')
    print_by_link(last_link(d, s) for d, s in deaths)

    print(f'  unplanned pushes: {len(pushes)}')
    print_by_link((p.link(), TRAVEL.get(int(p['type']), '?')) for p, s in pushes)

    hazards = histories(events, 'hazard')
    print(f'  entries into lava or slime: {len(hazards)}')
    print_by_link(last_link(h, s) for h, s in hazards)
    print(f'  pickups: {sum(pickups.values())}')
    for item, count in pickups.most_common(12):
        print(f'    {count:>5}  {item}')

    print(f'  stuck seconds: {sum(stuck.values())}')
    for link, count in stuck.most_common(5):
        print(f'    {count:>5}  {link}')

    changes = [e for e in events if e.kind == 'WEAPON']
    reasons = collections.Counter(e['reason'] for e in changes)
    print(f'  weapon changes: {len(changes)} ({len(changes) * per_minute:.2f} a minute); '
          + ', '.join(f'{reason} {n}' for reason, n in reasons.most_common()))
    flips = 0
    last = {}
    for e in changes:
        prev = last.get(e.client)
        if prev and prev['from'] == e['to'] and e.time - prev.time < 2000:
            flips += 1
        last[e.client] = e
    print(f'    changed back within 2 seconds: {flips}')
    for (a, b), n in collections.Counter((e['from'], e['to']) for e in changes).most_common(8):
        print(f'    {n:>5}  {a} -> {b}')

    gates = [e for e in events if e.kind == 'GATE']
    uses = collections.Counter((e['name'], e['result']) for e in events if e.kind == 'GATEUSE')
    print(f'  gates: {len(gates)}')
    for gate in gates:
        name = gate['name']
        print(f'    {name}: {gate["buttons"]} buttons, {gate["sources"]} nodes, {gate["doors"]} doors; '
              f'chosen {uses[name, "choose"]}, opened {uses[name, "open"]}, failed {uses[name, "fail"]}')


def cmd_summary(args):
    for path in args.logs:
        summarize(path, read_log(path))


def parse_link(spec):
    m = re.fullmatch(r'(\d+)-+>?(\d+)', spec or '')
    if spec and not m:
        sys.exit(f'--link must be A-B, such as 616-636: {spec}')
    return f'{m.group(1)}->{m.group(2)}' if m else None


def parse_near(spec):
    try:
        x, y, r = (float(v) for v in spec.split(','))
    except ValueError:
        sys.exit(f'--near must be X,Y,R, such as 0,800,300: {spec}')
    return x, y, r


def near(event, spec):
    if not spec:
        return True
    x, y, r = parse_near(spec)
    px, py, _ = event.pos()
    return math.hypot(px - x, py - y) <= r


def show(pairs, args):
    shown = 0
    wanted = parse_link(args.link)
    for event, samples in pairs:
        link, _ = last_link(event, samples)
        if wanted and link != wanted:
            continue
        if not near(event, args.near):
            continue
        print(event)
        for s in samples:
            print('   ', s)
        shown += 1
        if shown >= args.count:
            break


def cmd_deaths(args):
    show(histories(read_log(args.log), 'death'), args)


def cmd_pushes(args):
    show(histories(read_log(args.log), 'pushed'), args)


def cmd_hazards(args):
    show(histories(read_log(args.log), 'hazard'), args)


def cmd_node(args):
    nodes = read_graph(read_log(args.log))
    for i in args.ids:
        if i not in nodes:
            sys.exit(f'No node {i}: the graph has nodes 0 to {max(nodes)}')
        node = nodes[i]
        print(f'{i} {node["pos"]}' + (' edge' if node['edge'] else ''))
        for to, travel, cost in node['links']:
            gate = node['gates'].get(to)
            print(f'  -> {to:>5} {TRAVEL.get(travel, "?"):<9} cost {cost:>4} {nodes[to]["pos"] if to in nodes else "?"}' +
                  (f' through door *{gate}' if gate else ''))
        into = [(a, link_type(nodes, a, i)) for a, n in nodes.items() for to, _, _ in n['links'] if to == i]
        print(f'  <- ' + ', '.join(f'{a} {t}' for a, t in into))


def cmd_near(args):
    nodes = read_graph(read_log(args.log))
    point = (args.x, args.y, args.z)
    found = sorted((math.dist(n['pos'], point), i) for i, n in nodes.items())
    for distance, i in found:
        if distance > args.radius:
            break
        print(f'{i:>5} {nodes[i]["pos"]} distance {distance:.0f}, {len(nodes[i]["links"])} links' + (', edge' if nodes[i]['edge'] else ''))


def cmd_path(args):
    nodes = read_graph(read_log(args.log))
    for i in (args.start, args.to):
        if i not in nodes:
            sys.exit(f'No node {i}: the graph has nodes 0 to {max(nodes)}')
    dist, hop = shortest_paths_to(nodes, args.to)
    if args.start not in dist:
        sys.exit(f'No path from {args.start} to {args.to}')
    path = walk(hop, args.start)
    print(f'cost {dist[args.start]:.0f}')
    for a, b in zip(path, path[1:] + [None]):
        print(f'  {a:>5} {nodes[a]["pos"]}' + (' edge' if nodes[a]['edge'] else '') + (f'  {link_type(nodes, a, b)}' if b is not None else ''))


def cmd_routes(args):
    nodes = read_graph(read_log(args.log))
    if args.goal not in nodes:
        sys.exit(f'No node {args.goal}: the graph has nodes 0 to {max(nodes)}')
    dist, hop = shortest_paths_to(nodes, args.goal)
    through = set(args.nodes)
    crossing = [s for s in dist if s not in through and through & set(walk(hop, s))]
    print(f'{len(crossing)} of {len(dist)} nodes that reach {args.goal} route through {sorted(through)}')


# ---------------------------------------------------------------------------
# Players: commands, routes and kills
# ---------------------------------------------------------------------------

def humans(events):
    """The clients that the game recorded commands for: human players."""
    return sorted({e.client for e in events if e.kind == 'CMD'})


def client_names(events):
    names = {e.client: e['name'] for e in events if e.kind == 'SPEED'}
    for c in humans(events):
        names.setdefault(c, f'player {c}')
    return names


ROUTE_BREAK_DISTANCE = 192


def route_breaks(prev, event):
    """True if a client's route does not run from `prev` to `event`: a death or a teleport between."""
    return prev is None or math.dist(prev.pos(), event.pos()) > ROUTE_BREAK_DISTANCE


def pair(spec):
    return tuple(int(v) for v in spec.split(','))


def speed2(vel):
    return math.hypot(vel[0], vel[1])


def yaw_delta(a, b):
    return (b - a + 180.0) % 360.0 - 180.0


def median(values):
    values = sorted(values)
    return values[len(values) // 2] if values else 0


def coverage(visits):
    """Distinct 256x256x128 cells a minute, and the share of visits within 256 units of a visit
    20 to 60 seconds before, from a client's VISIT events."""
    if len(visits) < 2:
        return 0.0, 0.0
    minutes = collections.defaultdict(set)
    for v in visits:
        p = v.pos()
        minutes[v.time // 60000].add((p[0] // 256, p[1] // 256, p[2] // 128))
    back = 0
    start = 0
    for i, v in enumerate(visits):
        p = v.pos()
        while visits[start].time < v.time - 60000:
            start += 1
        if any(v.time - w.time >= 20000 and math.dist(p, w.pos()) < 256 for w in visits[start:i]):
            back += 1
    return sum(len(c) for c in minutes.values()) / len(minutes), back / len(visits)


def cmd_route(args):
    events = read_log(args.log)
    names = client_names(events)
    for client in sorted(names):
        if args.client is not None and client != args.client:
            continue
        visits = [e for e in events if e.kind == 'VISIT' and e.client == client]
        if not visits:
            continue
        cells, back = coverage(visits)
        nodes = collections.Counter(int(v['node']) for v in visits)
        print(f'{names[client]} ({client}): {len(visits)} visits, {len(nodes)} nodes, {cells:.1f} cells a minute, '
              f'{back:.0%} back within 256 units of a place 20 to 60 seconds before')
        print('  most visited: ' + ', '.join(f'{n} ({c})' for n, c in nodes.most_common(args.top)))
        if args.show:
            for v in visits[:args.show]:
                print(f'  {v.time:>8} {v["node"]:>5} {v["pos"]} speed {v["speed"]}')


def segments(events, client):
    """Each client's commands, grouped by the link between the VISIT before them and the next."""
    out = collections.defaultdict(list)
    last = None
    pending = []
    for e in events:
        if e.client != client:
            continue
        if e.kind == 'CMD':
            pending.append(e)
        elif e.kind == 'KILL':
            last = None
            pending = []
        elif e.kind == 'VISIT':
            if not route_breaks(last, e):
                out[(int(last['node']), int(e['node']))].append(pending)
            last = e
            pending = []
    return out


def runs_of(cmds):
    """Splits commands into runs of consecutive commands, at gaps of more than 100 ms."""
    out = []
    for c in cmds:
        if out and c.time - out[-1][-1].time <= 100:
            out[-1].append(c)
        else:
            out.append([c])
    return out


def describe_moves(runs):
    """Describes runs of consecutive commands: speed, ground, jumps, turn rate and keys."""
    cmds = [c for r in runs for c in r]
    steps = [(a, b) for r in runs for a, b in zip(r, r[1:])]
    n = len(cmds)
    speed = sum(speed2(c.pos('vel')) for c in cmds) / n
    ground = sum(c['ground'] == '1' for c in cmds) / n
    jumps = sum(1 for a, b in steps if a['ground'] == '1' and b['ground'] == '0' and pair(b['move'])[2] > 0)
    millis = sum(int(c['msec']) for c in cmds) or 1
    turn = sum(abs(yaw_delta(float(a['angles'].split(',')[0]), float(b['angles'].split(',')[0]))) for a, b in steps)
    moves = collections.Counter(tuple((v > 0) - (v < 0) for v in pair(c['move'])[:2]) for c in cmds)
    names = {(1, 0): 'fwd', (1, 1): 'fwd+right', (1, -1): 'fwd+left', (0, 1): 'right', (0, -1): 'left', (0, 0): 'none',
             (-1, 0): 'back', (-1, 1): 'back+right', (-1, -1): 'back+left'}
    keys = ', '.join(f'{names[k]} {v / n:.0%}' for k, v in moves.most_common(4))
    return (f'{n} commands, {millis / 1000:.1f}s, speed {speed:.0f}, ground {ground:.0%}, {jumps} jumps, '
            f'turn {turn * 1000 / millis:.0f} deg/s, keys: {keys}')


def cmd_moves(args):
    events = read_log(args.log)
    for client in humans(events):
        if args.client is not None and client != args.client:
            continue
        if args.node is not None:
            cmds = [e for e in events if e.kind == 'CMD' and e.client == client and int(e['node']) == args.node]
            if cmds:
                print(f'player {client} at node {args.node}: {describe_moves(runs_of(cmds))}')
            continue
        a, b = (int(v) for v in parse_link(args.link).split('->'))
        runs = segments(events, client).get((a, b), [])
        runs = [r for r in runs if r]
        if not runs:
            continue
        print(f'player {client} over {a}->{b}: {len(runs)} passes')
        print(f'  all: {describe_moves(runs)}')
        for r in runs[:args.count]:
            print(f'  {r[0].time:>8}: {describe_moves([r])}')
            if args.raw:
                for c in r:
                    print(f'      {c}')


def hops(cmds):
    """Each jump from the ground and its flight, from a player's commands."""
    out = []
    i = 0
    while i < len(cmds) - 1:
        a, b = cmds[i], cmds[i + 1]
        if not (a['ground'] == '1' and b['ground'] == '0' and pair(b['move'])[2] > 0):
            i += 1
            continue
        ground = 0
        j = i
        while j >= 0 and cmds[j]['ground'] == '1' and cmds[i].time - cmds[j].time < 1000:
            ground += int(cmds[j]['msec'])
            j -= 1
        k = i + 1
        while k < len(cmds) and cmds[k]['ground'] == '0' and cmds[k].time - b.time < 3000:
            k += 1
        if k >= len(cmds):
            break
        flight = cmds[i + 1:k + 1]
        if cmds[k]['ground'] == '0' or any(c['water'] != '0' or c.time - p.time > 100 for p, c in zip(cmds[i:k], flight)):
            i = k
            continue
        millis = sum(int(c['msec']) for c in flight) or 1
        turn = sum(abs(yaw_delta(float(x['angles'].split(',')[0]), float(y['angles'].split(',')[0]))) for x, y in zip(flight, flight[1:]))
        strafe = sum(1 for c in flight if pair(c['move'])[1] != 0) / len(flight)
        out.append({'takeoff': speed2(a.pos('vel')), 'landing': speed2(cmds[k].pos('vel')), 'air': millis,
                    'turn': turn * 1000 / millis, 'strafe': strafe, 'ground': ground, 'time': a.time})
        i = k
    return out


def cmd_strafe(args):
    events = read_log(args.log)
    for client in humans(events):
        if args.client is not None and client != args.client:
            continue
        cmds = [e for e in events if e.kind == 'CMD' and e.client == client]
        found = hops(cmds)
        if not found:
            continue
        print(f'player {client}: {len(found)} jumps')
        print(f'  {"takeoff":>9} {"count":>6} {"gain":>6} {"air ms":>7} {"turn/s":>7} {"strafe":>7} {"ground ms":>9}')
        for low in range(0, 900, 100):
            high = low + 100 if low < 800 else math.inf
            band = [h for h in found if low <= h['takeoff'] < high]
            if not band:
                continue
            n = len(band)
            label = f'{low}-{low + 99}' if high != math.inf else f'{low}+'
            print(f'  {label:>9} {n:>6} {sum(h["landing"] - h["takeoff"] for h in band) / n:>6.0f} '
                  f'{sum(h["air"] for h in band) / n:>7.0f} {sum(h["turn"] for h in band) / n:>7.0f} '
                  f'{sum(h["strafe"] for h in band) / n:>7.0%} {sum(h["ground"] for h in band) / n:>9.0f}')


def cmd_linktimes(args):
    times = {True: collections.defaultdict(list), False: collections.defaultdict(list)}
    counts = {}
    for path in args.logs:
        events = read_log(path)
        graph = next((e for e in events if e.kind == 'GRAPH'), None)
        counts[path] = graph['nodes'] if graph else '?'
        if len(set(counts.values()) - {'?'}) > 1:
            sys.exit(f'The logs have different graphs, so their node ids do not match: {counts}')
        people = set(humans(events))
        last = {}
        for e in events:
            if e.kind == 'KILL':
                last.pop(e.client, None)
                continue
            if e.kind != 'VISIT':
                continue
            prev = last.get(e.client)
            if not route_breaks(prev, e):
                times[e.client in people][(int(prev['node']), int(e['node']))].append(e.time - prev.time)
            last[e.client] = e
    rows = []
    for key, mine in times[True].items():
        theirs = times[False].get(key, [])
        if len(mine) >= args.min and len(theirs) >= args.min:
            rows.append((median(theirs) / max(1, median(mine)), key, median(mine), median(theirs), len(mine), len(theirs)))
    rows.sort(reverse=True)
    print(f'{len(rows)} node pairs that players and bots both crossed at least {args.min} times; bot time / player time:')
    for ratio, (a, b), mine, theirs, n, m in rows[:args.count]:
        print(f'  {a:>5}->{b:<5} player {mine:>5} ms ({n}), bots {theirs:>5} ms ({m}), {ratio:.1f}x')


MOD_FRIENDLY_FIRE = 0x8000000


def means_of_death(module):
    """The names of `GameMeansOfDeath`, from the game module's g_types.h, by value."""
    path = ROOT / 'src' / 'game' / module / 'g_types.h'
    names = {}
    if not path.exists():
        return names
    text = path.read_text(errors='replace')
    m = re.search(r'typedef enum \{([^}]*)\} GameMeansOfDeath;', text)
    if not m:
        return names
    value = 0
    for entry in re.findall(r'\b(MOD_\w+)(?:\s*=\s*(\w+))?', m.group(1)):
        if entry[1]:
            value = int(entry[1], 0)
        names[value] = entry[0]
        value += 1
    return names


def cmd_kills(args):
    events = read_log(args.log)
    names = means_of_death(args.module)
    kills = [e for e in events if e.kind == 'KILL']
    groups = collections.defaultdict(list)
    for k in kills:
        attacker = int(k['attacker'])
        kind = 'world' if attacker == -1 else 'suicide' if attacker == k.client else 'bot' if k['attackerBot'] == '1' else 'player'
        mod = int(k['mod']) & ~MOD_FRIENDLY_FIRE
        groups[(kind, names.get(mod, str(mod)), 'bot' if k['bot'] == '1' else 'player')].append(int(k['range']))
    print(f'{len(kills)} kills')
    print(f'  {"attacker":<8} {"means of death":<24} {"victim":<7} {"count":>5} {"median range":>12}')
    for (kind, mod, victim), ranges in sorted(groups.items(), key=lambda g: -len(g[1])):
        print(f'  {kind:<8} {mod:<24} {victim:<7} {len(ranges):>5} {median(ranges):>12}')


FLAG_EVENTS = ('steal', 'pickup', 'capture', 'return', 'drop', 'reset')


def cmd_flags(args):
    """Flag events per game minute over one or more logs, in total and by the team that did them,
    and the capture the flag points that they earned."""
    minutes = 0.0
    totals = collections.Counter()
    teams = collections.defaultdict(collections.Counter)
    kills = 0
    points = collections.Counter()
    awards = collections.Counter()
    for log in args.logs:
        events = read_log(log)
        minutes += game_seconds(events) / 60.0
        for e in events:
            if e.kind == 'POINTS':
                awards[e['event']] += 1
                points[e['event']] += int(e['points'])
            elif e.kind == 'FLAG':
                totals[e['event']] += 1
                if e['event'] != 'reset':
                    teams[e['team']][e['event']] += 1
            elif e.kind == 'KILL' and e.get('attacker') not in ('-1', str(e.client)):
                kills += 1
    if not minutes:
        sys.exit('No game time in the logs')
    print(f'{minutes:.1f} game minutes, {kills / minutes:.2f} kills/min')
    print(f'  {"":<8}' + ''.join(f'{name:>9}' for name in FLAG_EVENTS))
    print(f'  {"all":<8}' + ''.join(f'{totals[name] / minutes:>9.2f}' for name in FLAG_EVENTS))
    for team in sorted(teams):
        print(f'  {"team " + team:<8}' + ''.join(f'{teams[team][name] / minutes:>9.2f}' for name in FLAG_EVENTS))
    if awards:
        print(f'  {"points":<16}{"per min":>9}{"points/min":>12}')
        for name in sorted(awards, key=lambda n: -points[n]):
            print(f'  {name:<16}{awards[name] / minutes:>9.2f}{points[name] / minutes:>12.2f}')


# ---------------------------------------------------------------------------
# Suites
# ---------------------------------------------------------------------------

def cmd_suite(args):
    if args.jobs < 1:
        sys.exit('--jobs MUST be 1 or more')
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    bots = args.bot or SUITE_BOTS
    running = []
    failed = []

    def reap(limit):
        while len(running) > limit:
            done = [job for job in running if job[1].poll() is not None]
            if not done:
                time.sleep(0.5)
            for name, job, output in done:
                running.remove((name, job, output))
                output.close()
                if job.returncode:
                    failed.append(name)

    for i, name in enumerate(args.maps.split(',') if args.maps else SUITE_MAPS):
        reap(args.jobs - 1)
        command = [sys.executable, __file__, 'run', name, '--seconds', str(args.seconds),
                   '--port', str(args.port + i), '--out', str(out / name), '--module', args.module]
        command += [a for bot in bots for a in ('--bot', bot)]
        if args.graph:
            command.append('--graph')
        if args.tree:
            command += ['--tree', args.tree]
        command += args.extra
        output = open(out / f'{name}.txt', 'w')
        running.append((name, subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT), output))
    reap(0)

    if failed:
        sys.exit(f'These maps failed, see {out}/<map>.txt: {" ".join(failed)}')
    print(out)


def suite_logs(spec):
    """The profile log of each map, by map name, of the comma-separated suite directories."""
    logs = collections.defaultdict(list)
    for d in spec.split(','):
        for log in sorted(Path(d).glob('*/write/ai_profile_*.log')):
            logs[log.parent.parent.name].append(log)
    if not logs:
        sys.exit(f'No profile logs in {spec}')
    return logs


def suite_counts(logs):
    """Game minutes, world deaths, self kills, kills by players and pickups over the logs of one map."""
    counts = [0.0, 0, 0, 0, 0]
    for log in logs:
        events = read_log(log)
        counts[0] += game_seconds(events) / 60.0
        for e in events:
            if e.kind == 'KILL':
                if e.get('attacker') == '-1':
                    counts[1] += 1
                elif e.get('attacker') == str(e.client):
                    counts[2] += 1
                else:
                    counts[3] += 1
            elif e.kind == 'PICKUP':
                counts[4] += 1
    return counts


def cmd_compare(args):
    base, current = suite_logs(args.base), suite_logs(args.current)
    print('Each column holds the base value, then the current value.')
    print(f'{"map":20} {"minutes":>11} {"world/min":>14} {"self/min":>14} {"kills/min":>14} {"pickups/min":>14}')
    totals = [[0.0] * 5, [0.0] * 5]

    def row(name, b, c):
        if not b[0] or not c[0]:
            print(f'{name:20} no game time')
            return
        rates = ' '.join(f'{b[i] / b[0]:6.2f} {c[i] / c[0]:6.2f}' for i in range(1, 5))
        print(f'{name:20} {b[0]:5.0f} {c[0]:5.0f} {rates}')

    for name in sorted(set(base) | set(current)):
        if name not in base or name not in current:
            print(f'{name:20} only in {"the base" if name in base else "the current suite"}')
            continue
        b, c = suite_counts(base[name]), suite_counts(current[name])
        totals = [[t + v for t, v in zip(total, counts)] for total, counts in zip(totals, (b, c))]
        row(name, b, c)
    row('all', *totals)


def graph_lines(log):
    """The event and fields of each graph line, without the time and client."""
    with open(log, errors='replace') as f:
        return [(m.group(3), m.group(4)) for line in f
                if (m := LINE.match(line.rstrip('\n'))) and m.group(3) in GRAPH_EVENTS]


def cmd_graphdiff(args):
    if ',' in args.base + args.current:
        sys.exit('graphdiff compares one suite with one suite')
    base, current = suite_logs(args.base), suite_logs(args.current)
    differ = False
    for name in sorted(set(base) | set(current)):
        if name not in base or name not in current:
            print(f'{name:20} only in {"the base" if name in base else "the current suite"}')
            differ = True
            continue
        a, b = graph_lines(base[name][0]), graph_lines(current[name][0])
        if not a or not b:
            print(f'{name:20} no graph: record both suites with --graph')
            differ = True
        elif a == b:
            print(f'{name:20} same')
        else:
            first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'{name:20} differ: {len(a)} and {len(b)} lines, first difference at line {first + 1}')
            differ = True
    sys.exit(1 if differ else 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)

    p = sub.add_parser('run', help='run a headless match with the profiler')
    p.add_argument('map')
    p.add_argument('--seconds', type=float, default=60, help='wall seconds; with timeDemo, one bot on longyard plays about 170 game seconds in each')
    p.add_argument('--bot', action='append', default=[], help='NAME[:SKILL[,AGGRESSION[,AWARENESS]]], repeatable')
    p.add_argument('--clients', type=int, default=0, help='sv_minClients: roster bots that the game adds')
    p.add_argument('--graph', action='store_true', help='also record the navigation graph')
    p.add_argument('--module', default='default')
    p.add_argument('--port', type=int, default=27999)
    p.add_argument('--out', help='the directory for the stage, the write directory and server.log')
    p.add_argument('--tree', help='the tree whose build to run, by default this one')
    p.set_defaults(func=cmd_run)

    p = sub.add_parser('suite', help='a match on each map of a set, several at a time')
    p.add_argument('out', help='the directory for the matches, one directory and one .txt for each map')
    p.add_argument('--seconds', type=float, default=40, help='wall seconds of each match')
    p.add_argument('--bot', action='append', default=[],
                   help=f'NAME[:SKILL[,AGGRESSION[,AWARENESS]]], repeatable; by default {" ".join(SUITE_BOTS)}')
    p.add_argument('--maps', help='maps separated by commas, by default the 26 maps of quetoo-data')
    p.add_argument('--jobs', type=int, default=7, help='matches at the same time')
    p.add_argument('--graph', action='store_true', help='also record the navigation graph, for graphdiff')
    p.add_argument('--module', default='default')
    p.add_argument('--port', type=int, default=28100,
                   help='the port of the first map; each next map takes the next port')
    p.add_argument('--tree', help='the tree whose build to run, by default this one')
    p.set_defaults(func=cmd_suite)

    p = sub.add_parser('compare', help='deaths, kills and pickups per game minute of two suites, by map')
    p.add_argument('base', help='a suite directory, or several separated by commas')
    p.add_argument('current', help='a suite directory, or several separated by commas')
    p.set_defaults(func=cmd_compare)

    p = sub.add_parser('graphdiff', help='whether two suites recorded with --graph built the same graphs')
    p.add_argument('base', help='a suite directory')
    p.add_argument('current', help='a suite directory')
    p.set_defaults(func=cmd_graphdiff)

    p = sub.add_parser('summary', help='deaths, pushes, pickups and stuck time')
    p.add_argument('logs', nargs='+')
    p.set_defaults(func=cmd_summary)

    for name, func in (('deaths', cmd_deaths), ('pushes', cmd_pushes), ('hazards', cmd_hazards)):
        p = sub.add_parser(name, help=f'{name} with the 3.2 seconds of samples before each')
        p.add_argument('log')
        p.add_argument('--link', help='only the last link A-B, such as 616-636')
        p.add_argument('--near', help='only events within R units of X,Y')
        p.add_argument('--count', type=int, default=5)
        p.set_defaults(func=func)

    p = sub.add_parser('node', help='a node, its links and the links into it')
    p.add_argument('log')
    p.add_argument('ids', type=int, nargs='+')
    p.set_defaults(func=cmd_node)

    p = sub.add_parser('near', help='the nodes near a point')
    p.add_argument('log')
    p.add_argument('x', type=float)
    p.add_argument('y', type=float)
    p.add_argument('z', type=float)
    p.add_argument('--radius', type=float, default=128)
    p.set_defaults(func=cmd_near)

    p = sub.add_parser('path', help='the cheapest path between two nodes')
    p.add_argument('log')
    p.add_argument('start', type=int)
    p.add_argument('to', type=int)
    p.set_defaults(func=cmd_path)

    p = sub.add_parser('routes', help='how many nodes route to a goal through the given nodes')
    p.add_argument('log')
    p.add_argument('goal', type=int)
    p.add_argument('nodes', type=int, nargs='+')
    p.set_defaults(func=cmd_routes)

    p = sub.add_parser('route', help="each client's route, by the nodes it passed, and how it covers the map")
    p.add_argument('log')
    p.add_argument('--client', type=int)
    p.add_argument('--top', type=int, default=10)
    p.add_argument('--show', type=int, default=0, help='also print the first N visits')
    p.set_defaults(func=cmd_route)

    p = sub.add_parser('moves', help="a player's movement commands at a node or over a link")
    p.add_argument('log')
    p.add_argument('--client', type=int)
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument('--node', type=int)
    group.add_argument('--link', help='A-B')
    p.add_argument('--count', type=int, default=5)
    p.add_argument('--raw', action='store_true', help='also print each command')
    p.set_defaults(func=cmd_moves)

    p = sub.add_parser('strafe', help="a player's jumps by takeoff speed: speed gained, air time, turn rate and strafe keys")
    p.add_argument('log')
    p.add_argument('--client', type=int)
    p.set_defaults(func=cmd_strafe)

    p = sub.add_parser('linktimes', help='the node pairs that bots cross most slowly, against players')
    p.add_argument('logs', nargs='+', help='logs of the same build of the same map, such as a player\'s and a bot match')
    p.add_argument('--min', type=int, default=3, help='the least crossings of a pair by each')
    p.add_argument('--count', type=int, default=20)
    p.set_defaults(func=cmd_linktimes)

    p = sub.add_parser('kills', help='kills by attacker, means of death and victim, with the median range')
    p.add_argument('log')
    p.add_argument('--module', default='default', help='the game module whose means of death to name')
    p.set_defaults(func=cmd_kills)

    p = sub.add_parser('flags', help='CTF flag steals, pickups, captures, returns, drops and resets per game minute')
    p.add_argument('logs', nargs='+')
    p.set_defaults(func=cmd_flags)

    args, extra = parser.parse_known_args()
    if extra and args.command not in ('run', 'suite'):
        parser.error(f'unrecognized arguments: {" ".join(extra)}')
    unknown = [e for e in extra if e.startswith('--') and e != '--']
    if unknown:
        parser.error(f'unrecognized arguments: {" ".join(unknown)}')
    args.extra = [e for e in extra if e != '--']
    args.func(args)


if __name__ == '__main__':
    main()
