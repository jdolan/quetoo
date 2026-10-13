#!/usr/bin/env python3
"""
ai_probe.py - run headless bot matches and read the game's bot probe

The game module records what bots do when the developer cvar `g_aiProbe` is set (see
src/game/common/g_ai_probe.h): 1 records the bots, 2 the navigation graph, 3 both. It writes
`ai_probe_<map>.log` to the write directory. Each line is

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
