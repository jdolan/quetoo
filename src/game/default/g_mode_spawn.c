/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"
#include "bg_pmove.h"

/**
 * @brief Returns the distance to the nearest enemy from the given spot.
 */
static float G_EnemyRangeFromSpot(g_client_t *cl, g_entity_t *spot) {
  float dist, best_dist = 9999999.0;
  vec3_t v;

  G_ForEachClient(enemy, {
    if (!enemy->entity || enemy->entity->health <= 0 || enemy->persistent.spectator) {
      continue;
    }

    v = Vec3_Subtract(spot->s.origin, enemy->entity->s.origin);
    dist = Vec3_Length(v);

    if (G_ModeTeamplay() && enemy->persistent.team == cl->persistent.team && dist > 64.0) {
      continue;
    }

    if (dist < best_dist) {
      best_dist = dist;
    }
  });

  return best_dist;
}

/**
 * @brief Checks if spawning a player in this spot would cause a telefrag.
 */
static bool G_WouldTelefrag(const vec3_t spot) {
  g_entity_t *ents[MAX_ENTITIES];
  box3_t bounds = Box3_Translate(PM_BOUNDS, spot);

  bounds.mins.z -= PM_STEP_HEIGHT;
  bounds.maxs.z += PM_STEP_HEIGHT;

  const size_t len = gi.BoxEntities(bounds, ents, lengthof(ents), BOX_COLLIDE);
  for (size_t i = 0; i < len; i++) {
    if (G_IsMeat(ents[i])) {
      return true;
    }
  }
  return false;
}

/** @brief Selects a random unoccupied spawn point from the given set. */
static g_entity_t *G_SelectRandomSpawnPoint(const g_spawn_points_t *spawn_points) {
  if (!spawn_points->count) {
    assert(spawn_points != &g_level.spawn_points);
    return G_SelectRandomSpawnPoint(&g_level.spawn_points);
  }

  uint32_t empty_spawns[spawn_points->count];
  uint32_t num_empty_spawns = 0;
  for (uint32_t i = 0; i < spawn_points->count; i++) {
    if (!G_WouldTelefrag(spawn_points->spots[i]->s.origin)) {
      empty_spawns[num_empty_spawns++] = i;
    }
  }

  if (num_empty_spawns) {
    return spawn_points->spots[empty_spawns[RandomRangeu(0, num_empty_spawns)]];
  }
  return spawn_points->spots[RandomRangeu(0, spawn_points->count)];
}

/** @brief Adds unique spawn pointers from points into pool. */
static uint32_t G_CollectSpawnPoints(g_entity_t **pool, uint32_t count,
                                     const g_spawn_points_t *points) {
  for (uint32_t i = 0; i < points->count; i++) {
    g_entity_t *spot = points->spots[i];
    bool exists = false;
    for (uint32_t j = 0; j < count; j++) {
      if (pool[j] == spot) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      pool[count++] = spot;
    }
  }
  return count;
}

/** @brief Selects a random unoccupied point from a flat list. */
static g_entity_t *G_SelectRandomSpawnPointFromPool(g_entity_t **spots,
                                                     const uint32_t count) {
  if (!count) {
    return G_SelectRandomSpawnPoint(&g_level.spawn_points);
  }

  uint32_t empty_spawns[count];
  uint32_t num_empty_spawns = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (!G_WouldTelefrag(spots[i]->s.origin)) {
      empty_spawns[num_empty_spawns++] = i;
    }
  }

  if (num_empty_spawns) {
    return spots[empty_spawns[RandomRangeu(0, num_empty_spawns)]];
  }
  return spots[RandomRangeu(0, count)];
}

/** @brief Selects the point farthest from all enemies in a spawn set. */
static g_entity_t *G_SelectFarthestSpawnPoint(g_client_t *cl,
                                              const g_spawn_points_t *spawn_points) {
  g_entity_t *best_spot = NULL;
  float best_dist = 0.0;

  for (size_t i = 0; i < spawn_points->count; i++) {
    g_entity_t *spot = spawn_points->spots[i];
    const float dist = G_EnemyRangeFromSpot(cl, spot);
    if (dist > best_dist && !G_WouldTelefrag(spot->s.origin)) {
      best_spot = spot;
      best_dist = dist;
    }
  }

  return best_spot ?: G_SelectRandomSpawnPoint(spawn_points);
}

/** @brief Selects the farthest point from all enemies in a flat list. */
static g_entity_t *G_SelectFarthestSpawnPointFromPool(g_client_t *cl,
                                                      g_entity_t **spots,
                                                      const uint32_t count) {
  g_entity_t *best_spot = NULL;
  float best_dist = 0.0;

  for (uint32_t i = 0; i < count; i++) {
    g_entity_t *spot = spots[i];
    const float dist = G_EnemyRangeFromSpot(cl, spot);
    if (dist > best_dist && !G_WouldTelefrag(spot->s.origin)) {
      best_spot = spot;
      best_dist = dist;
    }
  }

  return best_spot ?: G_SelectRandomSpawnPointFromPool(spots, count);
}

/** @brief Selects an appropriate deathmatch spawn point. */
static g_entity_t *G_SelectDeathmatchSpawnPoint(g_client_t *cl) {
  g_entity_t *pool[MAX_ENTITIES];
  uint32_t count = G_CollectSpawnPoints(pool, 0, &g_level.spawn_points);

  // Include team spawns in non-team modes to improve map distribution.
  for (int32_t t = 0; t < MAX_TEAMS; t++) {
    count = G_CollectSpawnPoints(pool, count, &g_team_list[t].spawn_points);
  }

  if (g_spawn_farthest->value) {
    return G_SelectFarthestSpawnPointFromPool(cl, pool, count);
  }
  return G_SelectRandomSpawnPointFromPool(pool, count);
}

/** @brief Selects an appropriate team spawn point. */
static g_entity_t *G_SelectTeamSpawnPoint(g_client_t *cl) {
  if (!cl->persistent.team) {
    return NULL;
  }
  if (g_spawn_farthest->value) {
    return G_SelectFarthestSpawnPoint(cl, &cl->persistent.team->spawn_points);
  }
  return G_SelectRandomSpawnPoint(&cl->persistent.team->spawn_points);
}

/**
 * @brief Selects a spawn point through the mode seam, then common components.
 */
g_entity_t *G_SelectSpawnPoint(g_client_t *cl) {
  g_entity_t *mode_spawn = G_ModeSelectSpawn(cl);
  if (mode_spawn) {
    return mode_spawn;
  }

  g_entity_t *spawn = NULL;
  if (G_ModeTeamplay()) {
    spawn = G_SelectTeamSpawnPoint(cl);
  }
  return spawn ?: G_SelectDeathmatchSpawnPoint(cl);
}
