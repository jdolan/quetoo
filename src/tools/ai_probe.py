#!/usr/bin/env python3
"""
ai_probe.py - run headless bot matches and read the game's bot probe

The game module records what bots do when the developer cvar `g_aiProbe` is set (see
src/game/common/g_ai_probe.h): 1 records the bots, 2 the navigation graph, 3 both. It writes
`ai_probe_<map>.log` to the write directory. 4 records human players: each movement command
(CMD, with the view angles as yaw,pitch), each node passed (VISIT) and each pickup; 5 records the
bots and the players. Each line is

    PROBE <time> <client> <EVENT> <key>=<value> ...

Usage:
    ai_probe.py run <map> [--seconds 60] [--bot Bastard:0.8] [--clients N] [--graph] [--out DIR] [+set ...]
    ai_probe.py summary <log> [<log> ...]
    ai_probe.py deaths <log> [--link A-B] [--near X,Y,R] [--count N]
    ai_probe.py pushes <log> [--link A-B] [--count N]
    ai_probe.py node <log> <id> [<id> ...]
    ai_probe.py near <log> X Y Z [--radius R]
    ai_probe.py path <log> <from> <to>
    ai_probe.py routes <log> <goal> <node> [<node> ...]
    ai_probe.py route <log> [--client N] [--top 10] [--show N]
    ai_probe.py moves <log> (--node N | --link A-B) [--client N] [--count 5] [--raw]
    ai_probe.py strafe <log> [--client N]
    ai_probe.py linktimes <log> [--min 3] [--count 20]
    ai_probe.py kills <log> [--module default]

`run` builds the tree's dedicated server and game module into a private stage, so it never
touches the user directory. The `node`, `near`, `path` and `routes` commands need a log that was
recorded with `--graph`. Their path costs approximate `G_Ai_Node_StepCost`: the link cost, plus
the penalty for a drop over 128 units, the falling damage that a full-health bot takes, and the
penalty for a node at the edge of the void.
"""

import argparse
import collections
import heapq
import math
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

TRAVEL = {-1: 'none', 0: 'spliced', 1: 'walk', 2: 'crouch', 3: 'ledge', 4: 'barrier', 5: 'teleport',
          6: 'push', 7: 'plat', 8: 'swim', 9: 'jump'}

TRAVEL_PUSH = 6

RCON_PASSWORD = 'probe'

LINE = re.compile(r'^PROBE (\d+) (-?\d+) (\w+) ?(.*)$')


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
    """Pair each DEATH or PUSHED event with the HIST samples that the game writes right after it."""
    kind = 'DEATH' if reason == 'death' else 'PUSHED'
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
    return 'no path', 'none'


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
        if e['links']:
            for link in e['links'].split(','):
                to, travel, cost = (int(v) for v in link.split(':'))
                links.append((to, travel, cost))
        nodes[int(e['id'])] = {'pos': e.pos(), 'edge': e.get('edge') == '1', 'links': links}
    if not nodes:
        sys.exit('The log has no graph: record it with `run --graph`, or g_aiProbe 3')
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
    dedicated = ROOT / 'src/main/quetoo-dedicated'
    game = ROOT / f'src/game/{args.module}/.libs/game.so'
    cgame = ROOT / f'src/cgame/{args.module}/.libs/cgame.so'

    for path in (dedicated, game):
        if not path.exists():
            sys.exit(f'{path} is missing: build the tree with `make -j8` first')

    out = Path(args.out or tempfile.mkdtemp(prefix=f'ai_probe_{args.map}_')).resolve()
    stage = out / 'stage' / args.module
    write = out / 'write'
    stage.mkdir(parents=True, exist_ok=True)
    write.mkdir(parents=True, exist_ok=True)

    for link, target in ((stage / 'game.so', game), (stage / 'cgame.so', cgame)):
        if link.is_symlink() or link.exists():
            link.unlink()
        if target.exists():
            link.symlink_to(target)

    log = write / f'ai_probe_{args.map}.log'
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
               '+set', 'g_aiProbe', '3' if args.graph else '1',
               *args.extra,
               '+map', args.map]

    with open(out / 'server.log', 'w') as server_log:
        server = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=server_log, stderr=subprocess.STDOUT,
                                  cwd=ROOT)
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
        sys.exit(f'No probe log: see {out / "server.log"}')

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
    by_link = collections.Counter(last_link(d, s) for d, s in deaths)
    for (link, travel), count in by_link.most_common(10):
        print(f'    {count:>5}  {link} {travel}')

    print(f'  unplanned pushes: {len(pushes)}')
    by_link = collections.Counter((p.link(), TRAVEL.get(int(p['type']), '?')) for p, s in pushes)
    for (link, travel), count in by_link.most_common(10):
        print(f'    {count:>5}  {link} {travel}')

    print(f'  pickups: {sum(pickups.values())}')
    for item, count in pickups.most_common(12):
        print(f'    {count:>5}  {item}')

    print(f'  stuck seconds: {sum(stuck.values())}')
    for link, count in stuck.most_common(5):
        print(f'    {count:>5}  {link}')


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


def cmd_node(args):
    nodes = read_graph(read_log(args.log))
    for i in args.ids:
        if i not in nodes:
            sys.exit(f'No node {i}: the graph has nodes 0 to {max(nodes)}')
        node = nodes[i]
        print(f'{i} {node["pos"]}' + (' edge' if node['edge'] else ''))
        for to, travel, cost in node['links']:
            print(f'  -> {to:>5} {TRAVEL.get(travel, "?"):<9} cost {cost:>4} {nodes[to]["pos"] if to in nodes else "?"}')
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
    events = read_log(args.log)
    people = set(humans(events))
    times = {True: collections.defaultdict(list), False: collections.defaultdict(list)}
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


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)

    p = sub.add_parser('run', help='run a headless match with the probe')
    p.add_argument('map')
    p.add_argument('--seconds', type=float, default=60, help='wall seconds; with timeDemo, one bot on longyard plays about 170 game seconds in each')
    p.add_argument('--bot', action='append', default=[], help='NAME[:SKILL[,AGGRESSION[,AWARENESS]]], repeatable')
    p.add_argument('--clients', type=int, default=0, help='sv_minClients: roster bots that the game adds')
    p.add_argument('--graph', action='store_true', help='also record the navigation graph')
    p.add_argument('--module', default='default')
    p.add_argument('--port', type=int, default=27999)
    p.add_argument('--out', help='the directory for the stage, the write directory and server.log')
    p.set_defaults(func=cmd_run)

    p = sub.add_parser('summary', help='deaths, pushes, pickups and stuck time')
    p.add_argument('logs', nargs='+')
    p.set_defaults(func=cmd_summary)

    for name, func in (('deaths', cmd_deaths), ('pushes', cmd_pushes)):
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
    p.add_argument('log')
    p.add_argument('--min', type=int, default=3, help='the least crossings of a pair by each')
    p.add_argument('--count', type=int, default=20)
    p.set_defaults(func=cmd_linktimes)

    p = sub.add_parser('kills', help='kills by attacker, means of death and victim, with the median range')
    p.add_argument('log')
    p.add_argument('--module', default='default', help='the game module whose means of death to name')
    p.set_defaults(func=cmd_kills)

    args, extra = parser.parse_known_args()
    if extra and args.command != 'run':
        parser.error(f'unrecognized arguments: {" ".join(extra)}')
    unknown = [e for e in extra if e.startswith('--') and e != '--']
    if unknown:
        parser.error(f'unrecognized arguments: {" ".join(unknown)}')
    args.extra = [e for e in extra if e != '--']
    args.func(args)


if __name__ == '__main__':
    main()
