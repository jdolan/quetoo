/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "g_local.h"

#ifndef G_MODE_ENABLE_CTF
#define G_MODE_ENABLE_CTF 1
#endif

#if G_MODE_ENABLE_CTF

/**
 * @brief CTF-local entity state. The record is intentionally an AoS extension
 * of the runtime prefix, just like g_entity_s extends the server entity stub.
 */
typedef struct {
  g_mode_entity_t base;
  bool flag;
} g_ctf_entity_t;

typedef struct {
  g_mode_client_t base;
  uint8_t role;
  uint32_t captures;
} g_ctf_client_t;

typedef enum {
  G_CTF_FLAG_AT_BASE,
  G_CTF_FLAG_CARRIED,
  G_CTF_FLAG_DROPPED,
} g_ctf_flag_state_t;

typedef struct {
  g_team_id_t owner;
  g_ctf_flag_state_t state;
  g_entity_t *base;
  g_entity_t *dropped;
  g_client_t *carrier;
  uint32_t return_at;
} g_ctf_flag_t;

typedef struct {
  g_ctf_flag_t flags[MAX_TEAMS];
  uint32_t captures[MAX_TEAMS];
  struct {
    uint16_t capture;
    uint16_t return_sound;
    uint16_t steal;
  } sounds;
} g_ctf_state_t;

_Static_assert(offsetof(g_ctf_entity_t, base) == 0,
               "CTF entity data must begin with g_mode_entity_t");
_Static_assert(offsetof(g_ctf_client_t, base) == 0,
               "CTF client data must begin with g_mode_client_t");

static g_ctf_entity_t *G_CtfEntityData(g_mode_t *mode, const g_entity_t *ent) {
  if (!mode || !ent || !mode->entity_data) {
    return NULL;
  }
  g_ctf_entity_t *data = G_ModeEntityData(mode, ent->s.number);
  if (data->base.entity != ent || data->base.spawn_id != ent->s.spawn_id) {
    return NULL;
  }
  return data;
}

static g_ctf_flag_t *G_CtfStateFlag(g_mode_t *mode, const g_team_t *team) {
  if (!mode || !team || team->id < 0 || team->id >= MAX_TEAMS) {
    return NULL;
  }
  g_ctf_state_t *state = G_ModeState(mode);
  return state ? &state->flags[team->id] : NULL;
}

static g_ctf_state_t *G_CtfState(g_mode_t *mode) {
  return G_ModeState(mode);
}

static const g_mode_context_t *G_CtfContext(const g_mode_t *mode) {
  return G_ModeContext(mode);
}

/**
 * @brief Resolve a CTF flag through the mode-owned item registry.
 *
 * The legacy definitions are used only as immutable prototypes; active CTF
 * inventory tags are assigned by this mode instance and published to cgame.
 */
static const g_item_t *G_CtfFlagItem(g_mode_t *mode, const g_team_id_t id) {
  if (!mode || !mode->def || id < 0 || id >= MAX_TEAMS ||
      (size_t) id >= mode->def->num_items) {
    return NULL;
  }
  const g_mode_item_def_t *descriptor = &mode->def->items[id];
  if (descriptor->dynamic && mode->item_data && G_CtfContext(mode)->items) {
    return &G_CtfContext(mode)->items[mode->item_data[id].def.tag];
  }
  return descriptor->Resolve ? descriptor->Resolve(mode) : descriptor->item;
}

static g_team_id_t G_CtfFlagIndex(g_mode_t *mode, const g_item_t *item) {
  if (!mode || !item) {
    return -1;
  }
  const int32_t count = (int32_t) mode->def->num_items < MAX_TEAMS ?
      (int32_t) mode->def->num_items : MAX_TEAMS;
  for (int32_t i = 0; i < count; i++) {
    const g_item_t *flag = G_CtfFlagItem(mode, i);
    if (flag == item || (flag && flag->def.classname && item->def.classname &&
                         !q_strcmp(flag->def.classname, item->def.classname))) {
      return i;
    }
  }
  return -1;
}

static g_team_t *G_CtfTeamForFlag(const g_mode_context_t *context,
                                  const g_entity_t *ent) {
  if (!context || !context->level || !context->teams || !ent || !ent->item ||
      ent->item->def.type != ITEM_TYPE_FLAG) {
    return NULL;
  }

  int32_t num_teams = context->level->num_teams;
  if (num_teams <= 0) {
    num_teams = MAX_TEAMS;
  } else if (num_teams > MAX_TEAMS) {
    num_teams = MAX_TEAMS;
  }
  for (int32_t i = 0; i < num_teams; i++) {
    if (!q_strcmp(ent->classname, context->teams[i].flag)) {
      return &context->teams[i];
    }
  }

  return NULL;
}

static g_entity_t *G_CtfFlagForTeam(const g_team_t *team) {
  return team ? team->flag_entity : NULL;
}

static const g_item_t *G_CtfGetFlag(g_mode_t *mode, const g_client_t *cl);

static g_team_t *G_CtfTeamForFlagHook(g_mode_t *mode, const g_entity_t *ent) {
  return G_CtfTeamForFlag(G_CtfContext(mode), ent);
}

static g_entity_t *G_CtfFlagForTeamHook(g_mode_t *mode, const g_team_t *team) {
  (void) mode;
  return G_CtfFlagForTeam(team);
}

static const g_item_t *G_CtfGetFlagHook(g_mode_t *mode, const g_client_t *cl) {
  return G_CtfGetFlag(mode, cl);
}

static int32_t G_CtfEffectForTeamHook(g_mode_t *mode, const g_team_t *team) {
  (void) mode;
  return team ? team->effect : 0;
}

static const g_item_t *G_CtfGetFlag(g_mode_t *mode, const g_client_t *cl) {
  const g_mode_context_t *context = G_CtfContext(mode);
  if (!context || !cl) {
    return NULL;
  }

  for (int32_t i = 0; i < context->level->num_teams; i++) {
    if (&context->teams[i] == cl->persistent.team) {
      continue;
    }

    g_entity_t *flag = G_CtfFlagForTeam(&context->teams[i]);
    if (flag && cl->inventory[flag->item->def.tag]) {
      return flag->item;
    }
  }

  return NULL;
}

static void G_CtfResetDroppedFlag(g_mode_t *mode, g_entity_t *ent) {
  if (!G_CtfEntityData(mode, ent)) {
    return;
  }

  const g_mode_context_t *context = G_CtfContext(mode);
  g_team_t *team = G_CtfTeamForFlag(context, ent);
  g_entity_t *flag = G_CtfFlagForTeam(team);
  if (!team || !flag) {
    return;
  }

  g_ctf_flag_t *state = G_CtfStateFlag(mode, team);
  if (state) {
    state->state = G_CTF_FLAG_AT_BASE;
    state->dropped = NULL;
    state->carrier = NULL;
    state->return_at = 0;
  }

  flag->sv_flags &= ~SVF_NO_CLIENT;
  flag->s.event = EV_ITEM_RESPAWN;
  flag->s.event_data = flag->item->def.tag;
  flag->solid = SOLID_TRIGGER;

  gi.LinkEntity(flag);
  G_MulticastSound(&(const g_play_sound_t) {
    .index = G_CtfState(mode)->sounds.return_sound
  }, MULTICAST_PHS_R);
  gi.BroadcastPrint(PRINT_HIGH, "The %s flag has been returned :flag%d_return:\n",
                    team->name, team->id + 1);

  G_FreeEntity(ent);
}

static bool G_CtfPickupFlag(g_mode_t *mode, g_client_t *cl, g_entity_t *ent) {
  if (!G_CtfEntityData(mode, ent)) {
    return false;
  }

  if (!cl || !cl->persistent.team) {
    return false;
  }

  const g_mode_context_t *context = G_CtfContext(mode);
  g_team_t *team = G_CtfTeamForFlag(context, ent);
  g_entity_t *team_flag = G_CtfFlagForTeam(team);
  const g_item_t *carried_flag = G_CtfGetFlag(mode, cl);
  if (!team || !team_flag) {
    return false;
  }

  if (team == cl->persistent.team) {
    if (ent->spawn_flags & SF_ITEM_DROPPED) {
      g_ctf_flag_t *state = G_CtfStateFlag(mode, team);
      if (state) {
        state->state = G_CTF_FLAG_AT_BASE;
        state->dropped = NULL;
      }
      team_flag->solid = SOLID_TRIGGER;
      team_flag->sv_flags &= ~SVF_NO_CLIENT;
      gi.LinkEntity(team_flag);
      team_flag->s.event = EV_ITEM_RESPAWN;
      team_flag->s.event_data = team_flag->item->def.tag;
      G_MulticastSound(&(const g_play_sound_t) {
        .index = G_CtfState(mode)->sounds.return_sound
      }, MULTICAST_PHS);
      gi.BroadcastPrint(PRINT_HIGH, "%s returned the %s flag :flag%d_return:\n",
                        cl->persistent.net_name, team->name, team->id + 1);
      return true;
    }

    if (carried_flag) {
      const g_team_id_t other_id = G_CtfFlagIndex(mode, carried_flag);
      if (other_id < 0 || other_id >= context->level->num_teams) {
        return false;
      }
      const g_team_t *other_team = &context->teams[other_id];
      g_entity_t *other_flag = G_CtfFlagForTeam(other_team);
      const g_item_tag_t index = other_flag->item->def.tag;

      if (cl->inventory[index]) {
        g_ctf_flag_t *state = G_CtfStateFlag(mode, other_team);
        if (state) {
          state->state = G_CTF_FLAG_AT_BASE;
          state->carrier = NULL;
          state->dropped = NULL;
        }
        cl->inventory[index] = 0;
        cl->entity->s.effects &= ~other_team->effect;
        cl->entity->s.model3 = 0;
        other_flag->solid = SOLID_TRIGGER;
        other_flag->sv_flags &= ~SVF_NO_CLIENT;
        gi.LinkEntity(other_flag);
        other_flag->s.event = EV_ITEM_RESPAWN;
        other_flag->s.event_data = other_flag->item->def.tag;
        G_MulticastSound(&(const g_play_sound_t) {
          .index = G_CtfState(mode)->sounds.capture
        }, MULTICAST_PHS_R);
        gi.BroadcastPrint(PRINT_HIGH, "%s captured the %s flag :flag%d_capture:\n",
                          cl->persistent.net_name, other_team->name, other_team->id + 1);

        team->captures++;
        cl->persistent.captures++;
        g_ctf_state_t *ctf_state = G_ModeState(mode);
        if (ctf_state && team->id >= 0 && team->id < MAX_TEAMS) {
          ctf_state->captures[team->id]++;
        }

        const g_capture_t capture = {
          .player_ai = cl->ai != NULL,
          .time = (uint32_t) time(NULL),
        };
        g_capture_t record = capture;
        q_strlcpy(record.level, context->level->name, sizeof(record.level));
        q_strlcpy(record.player, cl->persistent.net_name, sizeof(record.player));
        q_strlcpy(record.player_guid, cl->persistent.guid, sizeof(record.player_guid));
        q_strlcpy(record.team, other_team->name, sizeof(record.team));
        if (record.player_guid[0]) {
          $(context->level->captures, add, &record);
        }
        return false;
      }
    }
    return false;
  }

  if (carried_flag) {
    return false;
  }

  team_flag->solid = SOLID_NOT;
  team_flag->sv_flags |= SVF_NO_CLIENT;
  g_ctf_flag_t *state = G_CtfStateFlag(mode, team);
  if (state) {
    state->state = G_CTF_FLAG_CARRIED;
    state->carrier = cl;
    state->dropped = NULL;
  }
  gi.LinkEntity(team_flag);
  const g_item_tag_t index = team_flag->item->def.tag;
  cl->inventory[index] = 1;
  cl->entity->s.model3 = team_flag->item->model_index;
  G_MulticastSound(&(const g_play_sound_t) {
    .index = G_CtfState(mode)->sounds.steal,
  }, MULTICAST_PHS);
  gi.BroadcastPrint(PRINT_HIGH, "%s stole the %s flag :flag%d_steal:\n",
                    cl->persistent.net_name, team->name, team->id + 1);
  cl->entity->s.effects |= team->effect;
  return true;
}

static g_entity_t *G_CtfDropFlag(g_mode_t *mode, g_client_t *cl) {
  const g_mode_context_t *context = G_CtfContext(mode);
  const g_item_t *flag = G_CtfGetFlag(mode, cl);
  if (!flag) {
    return NULL;
  }

  const g_team_id_t team_id = G_CtfFlagIndex(mode, flag);
  if (team_id < 0 || team_id >= context->level->num_teams) {
    return NULL;
  }
  const g_team_t *team = &context->teams[team_id];
  const g_item_tag_t index = flag->def.tag;
  if (!cl->inventory[index]) {
    return NULL;
  }

  cl->inventory[index] = 0;
  cl->entity->s.model3 = 0;
  cl->entity->s.effects &= ~EF_CTF_MASK;
  gi.BroadcastPrint(PRINT_HIGH, "%s dropped the %s flag :flag%d_drop:\n",
                    cl->persistent.net_name, team->name, team->id + 1);
  g_entity_t *dropped = G_DropItem(cl, flag);
  g_ctf_flag_t *state = G_CtfStateFlag(mode, team);
  if (state) {
    state->state = dropped ? G_CTF_FLAG_DROPPED : G_CTF_FLAG_AT_BASE;
    state->carrier = NULL;
    state->dropped = dropped;
    state->return_at = dropped ? context->level->time + 30000 : 0;
  }
  return dropped;
}

static bool G_CtfResetFlagItem(g_mode_t *mode, g_entity_t *ent) {
  const g_mode_context_t *context = G_CtfContext(mode);

  const g_team_id_t flag_team = G_CtfFlagIndex(mode, ent->item);
  if (!context->level->ctf || flag_team < 0 || flag_team >= context->level->num_teams) {
    ent->sv_flags |= SVF_NO_CLIENT;
    ent->solid = SOLID_NOT;
  }
  return true;
}

static void G_CtfClientBegin(g_mode_t *mode, g_client_t *cl) {
  g_ctf_client_t *data = G_ModeClientData(mode, cl->ps.client);
  data->role = cl->ai ? 1 : 0;
}

static void G_CtfEntitySpawn(g_mode_t *mode, g_entity_t *ent, void *raw) {
  g_ctf_entity_t *data = raw;
  data->flag = ent->item && ent->item->def.type == ITEM_TYPE_FLAG;
  if (data->flag) {
    const g_team_id_t id = G_CtfFlagIndex(mode, ent->item);
    if (id >= 0 && id < MAX_TEAMS) {
      g_ctf_state_t *state = G_ModeState(mode);
      if (state && !state->flags[id].base) {
        state->flags[id] = (g_ctf_flag_t) {
          .owner = id,
          .state = G_CTF_FLAG_AT_BASE,
          .base = ent,
        };
      }
    }
  }
}

static void G_CtfBotDirectives(g_mode_t *mode, g_client_t *cl,
                               const g_entity_t *ent,
                               g_mode_bot_directives_t *directives) {
  if (!cl || !ent || !ent->item || ent->item->def.type != ITEM_TYPE_FLAG ||
      !cl->persistent.team) {
    return;
  }

  g_ctf_client_t *client = G_ModeClientData(mode, cl->ps.client);
  client->role = G_CtfGetFlag(mode, cl) ? 2 : 1;

  const g_team_id_t flag_team = G_CtfFlagIndex(mode, ent->item);
  if (flag_team == cl->persistent.team->id) {
    directives->item_weight *= (ent->spawn_flags & SF_ITEM_DROPPED) ? 3.f : .25f;
  } else {
    directives->item_weight *= 2.f;
  }
}

static void G_CtfBotTarget(g_mode_t *mode, const g_client_t *cl,
                           const g_entity_t *target, float *priority,
                           float *chase_multiplier) {
  (void) cl;
  if (!target || !target->client) {
    return;
  }

  if (G_CtfGetFlag(mode, target->client)) {
    if (priority) {
      *priority += 5.f;
    }
    if (chase_multiplier) {
      *chase_multiplier *= 2.f;
    }
  }
}

static bool G_CtfBotCanPickup(g_mode_t *mode, const g_client_t *cl,
                              const g_entity_t *item, bool *can_pickup) {
  (void) mode;
  if (!cl || !item || !item->item || item->item->def.type != ITEM_TYPE_FLAG ||
      !cl->persistent.team) {
    return false;
  }

  const g_team_id_t flag_team = G_CtfFlagIndex(mode, item->item);
  if (flag_team == cl->persistent.team->id && item->owner == NULL) {
    *can_pickup = false;
  } else {
    *can_pickup = true;
  }
  return true;
}

static bool G_CtfCheckRules(g_mode_t *mode) {
  const g_mode_context_t *context = G_CtfContext(mode);
  if (!context->level->capture_limit) {
    return false;
  }
  g_ctf_state_t *state = G_ModeState(mode);
  if (!state) {
    return false;
  }
  for (int32_t i = 0; i < context->level->num_teams; i++) {
    if (state->captures[i] >= (uint32_t) context->level->capture_limit) {
      gi.BroadcastPrint(PRINT_HIGH, "Capture limit hit\n");
      return true;
    }
  }
  return false;
}

static void G_CtfLevelBegin(g_mode_t *mode, const char *map_name,
                            const cm_entity_t *props) {
  (void) map_name;
  (void) props;
  g_ctf_state_t *state = G_CtfState(mode);
  state->sounds.capture = gi.SoundIndex("ctf/capture");
  state->sounds.return_sound = gi.SoundIndex("ctf/return");
  state->sounds.steal = gi.SoundIndex("ctf/steal");
}

static void G_CtfSpawnFlagEntity(g_mode_t *mode, g_entity_t *ent, void *data) {
  (void) data;

  g_team_id_t id = 0;
  if (!q_strcmp(ent->classname, "item_flag_team2")) {
    id = 1;
  } else if (!q_strcmp(ent->classname, "item_flag_team3")) {
    id = 2;
  } else if (!q_strcmp(ent->classname, "item_flag_team4")) {
    id = 3;
  }
  const g_item_t *item = G_CtfFlagItem(mode, id);
  if (item) {
    G_SpawnItem(ent, item);
  }
}

static const g_item_t *G_CtfResolveFlag(g_mode_t *mode, const g_item_tag_t tag) {
  const g_mode_context_t *context = G_CtfContext(mode);
  return context && context->items ? &context->items[tag] : NULL;
}

static const g_item_t *G_CtfResolveRedFlag(g_mode_t *mode) {
  return G_CtfResolveFlag(mode, FLAG_RED);
}

static const g_item_t *G_CtfResolveBlueFlag(g_mode_t *mode) {
  return G_CtfResolveFlag(mode, FLAG_BLUE);
}

static const g_item_t *G_CtfResolveYellowFlag(g_mode_t *mode) {
  return G_CtfResolveFlag(mode, FLAG_YELLOW);
}

static const g_item_t *G_CtfResolveGreenFlag(g_mode_t *mode) {
  return G_CtfResolveFlag(mode, FLAG_GREEN);
}

static const g_mode_entity_class_def_t g_ctf_entity_classes[] = {
  { "item_flag_team1", G_CtfSpawnFlagEntity, NULL },
  { "item_flag_team2", G_CtfSpawnFlagEntity, NULL },
  { "item_flag_team3", G_CtfSpawnFlagEntity, NULL },
  { "item_flag_team4", G_CtfSpawnFlagEntity, NULL },
};

static const g_mode_item_def_t g_ctf_items[] = {
  { .classname = "item_flag_team1", .Resolve = G_CtfResolveRedFlag, .dynamic = true },
  { .classname = "item_flag_team2", .Resolve = G_CtfResolveBlueFlag, .dynamic = true },
  { .classname = "item_flag_team3", .Resolve = G_CtfResolveYellowFlag, .dynamic = true },
  { .classname = "item_flag_team4", .Resolve = G_CtfResolveGreenFlag, .dynamic = true },
};

static const g_mode_ops_t g_ctf_mode_ops = {
  .LevelBegin = G_CtfLevelBegin,
  .ClientBegin = G_CtfClientBegin,
  .EntitySpawn = G_CtfEntitySpawn,
  .ItemPickup = G_CtfPickupFlag,
  .ItemDrop = G_CtfDropFlag,
  .ItemResetDropped = G_CtfResetDroppedFlag,
  .ItemReset = G_CtfResetFlagItem,
  .TeamForFlag = G_CtfTeamForFlagHook,
  .FlagForTeam = G_CtfFlagForTeamHook,
  .GetFlag = G_CtfGetFlagHook,
  .EffectForTeam = G_CtfEffectForTeamHook,
  .BotDirectives = G_CtfBotDirectives,
  .BotTarget = G_CtfBotTarget,
  .BotCanPickup = G_CtfBotCanPickup,
  .CheckRules = G_CtfCheckRules,
};

static const g_mode_cvar_def_t g_ctf_cvars[] = {
  {
    .name = "g_ctf",
    .default_value = "0",
    .flags = CVAR_SERVER_INFO,
    .description = "Enables capture the flag gameplay.",
  },
  {
    .name = "g_capture_limit",
    .default_value = "8",
    .flags = CVAR_SERVER_INFO,
    .description = "The capture limit per level.",
  },
};

static const g_mode_def_t g_ctf_mode = {
  .name = "ctf",
  .kind = G_MODE_PRIMARY,
  .gameplay = GAME_DEATHMATCH,
  .capabilities = G_MODE_CAP_TEAMPLAY | G_MODE_CAP_FLAG_OBJECTIVE,
  .state_size = sizeof(g_ctf_state_t),
  .entity_data_size = sizeof(g_ctf_entity_t),
  .client_data_size = sizeof(g_ctf_client_t),
  .objective_cvar = "g_ctf",
  .capture_limit_cvar = "g_capture_limit",
  .cvars = g_ctf_cvars,
  .num_cvars = lengthof(g_ctf_cvars),
  .entity_classes = g_ctf_entity_classes,
  .num_entity_classes = lengthof(g_ctf_entity_classes),
  .items = g_ctf_items,
  .num_items = lengthof(g_ctf_items),
  .ops = &g_ctf_mode_ops,
};

const g_mode_def_t *G_CtfModeDefinition(void) {
  return &g_ctf_mode;
}

#endif /* G_MODE_ENABLE_CTF */
