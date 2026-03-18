# AI / Bot System (game/default/g_ai*.c)

Bot AI system for autonomous players with pathfinding, combat, and goal selection.

## Location

`src/game/default/g_ai*.c` - AI code within game module

## Core Purpose

- Autonomous bot players for single-player and practice
- Navigation using pathfinding (A* algorithm)
- Combat AI (targeting, dodging, weapon selection)
- Goal-driven behavior (attack, defend, collect items)
- Team play support

## Key Files

### g_ai_main.c / g_ai_main.h
Main AI loop and high-level logic:
- `Ai_Think()` - Main AI tick (called every frame per bot)
- Target selection (enemy tracking)
- Line of sight checks
- Command execution (as if bot typed command)

### g_ai_node.c / g_ai_node.h
Navigation node system:
- `Ai_Node_Init()` - Build navigation graph from map
- Nodes placed automatically at item spawns, key locations
- Links connect reachable nodes (considering jumps, falls)
- A* pathfinding algorithm

### g_ai_goal.c / g_ai_goal.h
Goal/objective system:
- `Ai_Goal_Think()` - Select and pursue goals
- Goal types: attack enemy, collect item, defend position
- Goal weighting (priority system)
- State machine for goal execution

### g_ai_item.c / g_ai_item.h
Item collection:
- `Ai_Item_Pick()` - Choose which item to pursue
- Item weighting (health/armor when low, weapons, powerups)
- Distance and reachability checks
- Item priority (mega health > health, etc.)

### g_ai_info.c / g_ai_info.h
AI perception and world knowledge:
- Enemy tracking (last seen position, time)
- Item memory (remember item locations)
- Danger assessment (low health, enemy has advantage)

## AI Structure

```c
typedef struct g_ai_s {
    g_client_t *client;          // The bot's client
    
    // Perception
    vec3_t eye_origin;           // View position
    vec3_t aim_forward;          // Aim direction
    
    // Navigation
    ai_node_t *current_node;     // Current nav node
    ai_node_t *goal_node;        // Target nav node
    ai_node_t *path[MAX_NODES];  // Path to goal
    int32_t path_index;
    
    // Combat
    g_entity_t *enemy;           // Current target
    vec3_t enemy_last_pos;       // Last known position
    uint32_t enemy_time;         // Last time saw enemy
    
    // Goals
    ai_goal_t *goal;             // Current goal
    
    // Weapons
    const g_item_t *weapon;      // Preferred weapon
    
    // Timing
    uint32_t think_time;         // Next think time
    uint32_t reaction_time;      // Reaction delay
} g_ai_t;
```

## Navigation System

### Node Graph

Nodes placed at strategic locations:
- Item spawn points (health, armor, weapons)
- Player spawn points
- Key map locations (junctions, platforms)

Links between nodes:
- Walking distance
- Jump required?
- Fall drop?
- Special movement (teleporter, ladder)?

### Pathfinding (A*)

```c
ai_node_t *Ai_Node_FindPath(ai_node_t *from, ai_node_t *to) {
    // A* algorithm
    // Heuristic: straight-line distance
    // Cost: actual path distance
    // Returns: path as linked list of nodes
}
```

Bot follows path by moving toward current node, then advances to next.

### Dynamic Obstacles

- Doors, platforms handled automatically (wait for door to open)
- Players blocking path: bot tries alternate route or waits
- Unreachable goal: abandon after timeout

## Combat AI

### Target Selection

Priority order:
1. **Current enemy** (if still alive and visible)
2. **Closest visible enemy** (within FOV and line of sight)
3. **Last known position** (hunt enemy)
4. **No target** (pursue other goals)

```c
bool Ai_CanSee(const g_client_t *cl, const g_entity_t *other) {
    // Check FOV (facing direction)
    // Trace line of sight
    // Return true if visible
}
```

### Aiming

- Lead moving targets (predict position)
- Aim for center mass
- Add inaccuracy (skill-based)
- Weapon-specific adjustments (projectile speed)

### Weapon Selection

Priority depends on situation:
1. **Railgun** - Long range, clear shot
2. **Rocket launcher** - Medium range, splash damage
3. **Lightning gun** - Close range, high DPS
4. **Shotgun** - Close range, high burst
5. **Blaster** - Fallback

Considers:
- Available ammo
- Distance to target
- Weapon effectiveness

### Dodging

- Strafe randomly while engaging
- Jump to avoid projectiles
- Back off when low health
- Use cover (hide behind obstacles)

## Goal System

### Goal Types

**GOAL_ATTACK**: Hunt and kill enemy
- Find path to enemy
- Chase if enemy flees
- Engage at optimal range for weapon

**GOAL_DEFEND**: Hold position (CTF flag, etc.)
- Stay near defend point
- Engage enemies that approach
- Don't chase far from defend position

**GOAL_ITEM**: Collect item
- Find path to item
- Navigate to item location
- Pick up when reached

### Goal Weighting

```c
float Ai_Item_Weight(const g_entity_t *item) {
    float weight = 0.0;
    
    if (item->item->type == ITEM_HEALTH) {
        if (bot->health < 50) {
            weight = 10.0;  // High priority when low
        }
    }
    
    if (item->item->type == ITEM_WEAPON) {
        if (!bot->has_weapon) {
            weight = 15.0;  // High priority if don't have
        }
    }
    
    // Distance penalty
    float dist = Vec3_Distance(bot->origin, item->origin);
    weight *= (1.0 - dist / MAX_ITEM_DISTANCE);
    
    return weight;
}
```

Highest weight goal wins.

## Skill Levels

Bots have skill setting (0-100) affecting:
- **Reaction time**: Lower skill = slower reactions
- **Aim accuracy**: Lower skill = more aim error
- **Strafe frequency**: Lower skill = less dodging
- **Weapon selection**: Lower skill = suboptimal weapons

Set with: `skill 75` (0-100 scale)

## Common Patterns

### Adding a Bot

```
sv addbot          # Add bot with random name
sv addbot "BotName"  # Add bot with specific name
```

Server spawns bot as if player connected:
1. Allocate client slot
2. Create `g_ai_t` structure
3. Link to `g_client_t`
4. Spawn entity
5. Bot starts thinking next frame

### Removing a Bot

```
sv removebot "BotName"
```

Bot disconnects like normal player.

### Bot Commands

Bots execute console commands:
```c
Ai_Command(bot, "kill");           // Suicide
Ai_Command(bot, "team red");       // Join team
Ai_Command(bot, "say_team Help!"); // Team chat
```

## Node Development

### Viewing Nodes

```
ai_node_dev 1     # Show navigation nodes as sprites
```

Useful for:
- Verifying node placement
- Debugging pathfinding
- Identifying gaps in coverage

### Automatic Node Generation

Nodes generated at map load:
1. Find all item spawn points
2. Add player spawn points
3. Sample map geometry for key locations
4. Build links between reachable nodes
5. Test links (can bot walk/jump from A to B?)

## Performance

### CPU Usage

- Each bot: ~0.5-1% CPU (typical)
- 16 bots: ~8-16% CPU (significant)
- Most expensive: pathfinding, line-of-sight traces

### Optimization Techniques

- Think every N frames (not every frame)
- Cache line-of-sight results
- Limit pathfinding frequency
- Use spatial hashing for enemy search

## Console Variables

```
ai_no_target 0        # Bots don't target players (testing)
ai_node_dev 0         # Show navigation nodes
skill 50              # Bot skill (0-100)
```

## Common Issues

### Bots Getting Stuck

- Path blocked by dynamic obstacle (door)
- Node graph incomplete (missing link)
- Bot cornered by geometry
- Solution: Improve node placement, add timeout to abandon goal

### Bots Not Fighting

- Target selection broken (check line of sight)
- Weapon out of ammo (check weapon selection)
- Goal priority wrong (combat goal should be high weight)

### Bots Ignoring Items

- Item not in node graph
- Path to item unreachable
- Other goals higher priority
- Check item weighting

### Poor Pathfinding

- Nodes too sparse (add more nodes)
- Missing links (bot can reach but no link)
- Incorrect link costs (jump vs walk)
- Use `ai_node_dev 1` to visualize

## Team Play

Bots support team modes:
- Join team with `team red` or `team blue`
- Defend/attack based on team goals
- Coordinate with teammates (TODO: more coordination)

## Future Improvements

Potential AI enhancements:
- **Teamwork**: Coordinated attacks, passing items
- **Map learning**: Remember good positions, ambush spots
- **Personality**: Aggressive vs defensive playstyles
- **Voice chat**: Bots announce actions ("I've got the flag!")

## Integration

- Game module spawns bots like players
- Server treats bots as normal clients (sends frames, processes commands)
- Main difference: Bot input comes from AI, not network

## Debugging

```
ai_node_dev 1         # Visualize navigation
g_developer 1         # AI debug output
sv_no_vis 1           # Disable PVS (see through walls)
```

Add debug output:
```c
gi.DebugPrint("Bot %s targeting %s\n", bot->name, enemy->name);
```
