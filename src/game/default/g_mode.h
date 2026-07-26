/*
 * Copyright(c) 2026 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "g_types.h"

#if defined(__GAME_LOCAL_H__)

/**
 * @brief The kind of module being composed for a level.
 */
typedef enum {
  G_MODE_PRIMARY,
  G_MODE_MODIFIER
} g_mode_kind_t;

/**
 * @brief Capabilities a composed mode may provide to common components.
 *
 * These are deliberately behavioral capabilities rather than mode names, so
 * common code does not need to depend on CTF, Instagib, or Arena symbols.
 */
typedef enum {
  G_MODE_CAP_TEAMPLAY = 1u << 0,
  G_MODE_CAP_FLAG_OBJECTIVE = 1u << 1,
  G_MODE_CAP_INSTAGIB = 1u << 2,
  G_MODE_CAP_ARENA = 1u << 3,
  G_MODE_CAP_NO_AMMO = 1u << 4,
  G_MODE_CAP_NO_SELF_DAMAGE = 1u << 5,
  G_MODE_CAP_SUPPRESS_ITEMS = 1u << 6,
} g_mode_capability_t;

typedef struct g_mode_s g_mode_t;
typedef struct g_mode_def_s g_mode_def_t;

/** @brief Maximum number of independently composable modifier modules. */
#define G_MODE_MAX_MODIFIERS 4

/**
 * @brief Common services exposed to a mode instance.
 *
 * The runtime owns this view and refreshes it for each level. Mode code uses
 * the view instead of reaching into the legacy process globals directly;
 * this keeps the module's mutable state in its instance while allowing an
 * incremental migration of the surrounding game code.
 */
typedef struct {
  g_level_t *level;
  g_media_t *media;
  g_team_t *teams;
  g_item_t *items;
  g_entity_t **entities;
  g_client_t **clients;
  size_t max_entities;
  size_t max_clients;
} g_mode_context_t;

/**
 * @brief Common prefix for a mode-owned entity record.
 *
 * Mode implementations may extend this structure, but it must remain the
 * first member of their record so the runtime can initialize slot identity.
 */
typedef struct {
  g_entity_t *entity;
  uint8_t spawn_id;
} g_mode_entity_t;

/**
 * @brief Common prefix for a mode-owned client record.
 */
typedef struct {
  g_client_t *client;
  uint32_t generation;
} g_mode_client_t;

/**
 * @brief Declarative cvar owned by a compiled mode.
 */
typedef struct {
  const char *name;
  const char *default_value;
  uint32_t flags;
  const char *description;
} g_mode_cvar_def_t;

/**
 * @brief Read-only bot policy output consumed by the generic AI planner.
 */
typedef struct {
  float item_weight;
  int16_t role;
} g_mode_bot_directives_t;

/**
 * @brief Mode-owned map entity class registration.
 */
typedef struct {
  const char *classname;
  void (*Spawn)(g_mode_t *mode, g_entity_t *ent, void *data);
  void (*Destroy)(g_mode_t *mode, g_entity_t *ent, void *data);
} g_mode_entity_class_def_t;

/**
 * @brief Mode-owned item registration. Non-dynamic entries alias a built-in
 * catalog item; dynamic entries treat `item` (or the resolver result) as an
 * immutable prototype and receive an owner-scoped runtime tag.
 */
typedef struct {
  const char *classname;
  const g_item_t *item;
  const g_item_t *(*Resolve)(g_mode_t *mode);
  /**
   * @brief Clone `item` into a runtime-owned inventory slot.
   *
   * A false value keeps the descriptor as an alias to a built-in item. This
   * makes mode extensions explicit while preserving the existing catalog.
  */
  bool dynamic;
  /** @brief Resolve a mode-owned ammo item after all runtime tags exist. */
  const g_item_t *(*ResolveAmmo)(g_mode_t *mode);
} g_mode_item_def_t;

/**
 * @brief Hooks exported by every mode implementation.
 *
 * Hooks receive an opaque mode instance. A mode may keep mutable state in the
 * instance, but must not expose or use a mutable file-global singleton.
 */
typedef struct {
  void (*LevelBegin)(g_mode_t *mode, const char *map_name, const cm_entity_t *props);
  void (*LevelEnd)(g_mode_t *mode);
  void (*Frame)(g_mode_t *mode);
  void (*ResetItems)(g_mode_t *mode);
  void (*ClientFrame)(g_mode_t *mode, g_client_t *cl);
  void (*ClientBegin)(g_mode_t *mode, g_client_t *cl);
  void (*ClientDisconnect)(g_mode_t *mode, g_client_t *cl);
  bool (*ClientInventory)(g_mode_t *mode, g_client_t *cl,
                         const g_item_t **starting_weapon);
  void (*EntitySpawn)(g_mode_t *mode, g_entity_t *ent, void *data);
  void (*EntityFree)(g_mode_t *mode, g_entity_t *ent, void *data);
  bool (*ItemPickup)(g_mode_t *mode, g_client_t *cl, g_entity_t *ent);
  g_entity_t *(*ItemDrop)(g_mode_t *mode, g_client_t *cl);
  void (*ItemResetDropped)(g_mode_t *mode, g_entity_t *ent);
  bool (*ItemReset)(g_mode_t *mode, g_entity_t *ent);
  g_team_t *(*TeamForFlag)(g_mode_t *mode, const g_entity_t *ent);
  g_entity_t *(*FlagForTeam)(g_mode_t *mode, const g_team_t *team);
  const g_item_t *(*GetFlag)(g_mode_t *mode, const g_client_t *cl);
  int32_t (*EffectForTeam)(g_mode_t *mode, const g_team_t *team);
  void (*BotDirectives)(g_mode_t *mode, g_client_t *cl,
                        const g_entity_t *ent, g_mode_bot_directives_t *directives);
  void (*BotTarget)(g_mode_t *mode, const g_client_t *cl,
                    const g_entity_t *target, float *priority,
                    float *chase_multiplier);
  bool (*BotCanPickup)(g_mode_t *mode, const g_client_t *cl,
                       const g_entity_t *item, bool *can_pickup);
  void (*ModifyDamage)(g_mode_t *mode, g_damage_t *damage, bool *cancel);
  uint32_t (*ModifyWeaponInterval)(g_mode_t *mode, g_client_t *cl,
                                   uint32_t interval);
  void (*DamageApplied)(g_mode_t *mode, const g_damage_t *damage,
                        int32_t damage_health, bool was_dead);
  bool (*Respawn)(g_mode_t *mode, g_client_t *cl, bool voluntary);
  g_entity_t *(*SelectSpawn)(g_mode_t *mode, g_client_t *cl);
  bool (*AssignTeam)(g_mode_t *mode, g_client_t *cl);
  bool (*CheckRules)(g_mode_t *mode);
} g_mode_ops_t;

/**
 * @brief Immutable mode metadata exported by a mode translation unit.
 */
struct g_mode_def_s {
  const char *name;
  g_mode_kind_t kind;
  /**
   * @brief True when `gameplay` is selected through the legacy g_gameplay
   * setting. Independent modifiers leave this false and are composed
   * explicitly by worldspawn.
   */
  bool gameplay_selector;
  g_gameplay_t gameplay;
  uint32_t capabilities;
  /**
   * @brief Size of the single mode-wide state record.
   *
   * The runtime allocates one zeroed record per active mode instance and
   * level. Use it for relationships shared by the whole mode, such as the one
   * client who is "it" in Tag. This is owner-scoped state, not a mutable
   * process-global singleton.
   */
  size_t state_size;
  size_t entity_data_size;
  size_t client_data_size;
  /** @brief Optional cvar selecting this objective mode. */
  const char *objective_cvar;
  /** @brief Optional cvar providing this mode's capture/score limit. */
  const char *capture_limit_cvar;
  const g_mode_cvar_def_t *cvars;
  size_t num_cvars;
  const g_mode_entity_class_def_t *entity_classes;
  size_t num_entity_classes;
  const g_mode_item_def_t *items;
  size_t num_items;
  const g_mode_ops_t *ops;
};

/**
 * @brief Active mode instance owned by the mode runtime.
 */
struct g_mode_s {
  const g_mode_def_t *def;
  const g_mode_context_t *context;
  void *state;
  void *entity_data;
  void *client_data;
  g_item_t *item_data;
  size_t num_item_data;
  size_t entity_stride;
  size_t client_stride;
  uint32_t generation;
};

/**
 * @brief Initialize and shut down the mode registry.
 */
void G_ModeInit(void);
void G_ModeShutdown(void);

/**
 * @brief Activate the named mode for the current level.
 */
void G_ModeBeginLevel(const char *mode_name,
                      const char *const *modifier_names,
                      size_t num_modifiers,
                      const char *map_name, const cm_entity_t *props);
void G_ModeEndLevel(void);

/**
 * @brief Dispatch common lifecycle events to the active mode.
 */
void G_ModeFrame(void);
void G_ModeResetItems(void);
void G_ModeClientFrame(g_client_t *cl);
bool G_ModeResolveTechs(g_level_t *level, const cm_entity_t *world);
cvar_t *G_ModeTechsCvar(void);
void G_ModeClientBegin(g_client_t *cl);
void G_ModeClientDisconnect(g_client_t *cl);
bool G_ModeClientInventory(g_client_t *cl, const g_item_t **starting_weapon);

/**
 * @brief Dispatch mode-owned item behavior.
 */
bool G_ModeItemPickup(g_client_t *cl, g_entity_t *ent);
g_entity_t *G_ModeItemDrop(g_client_t *cl);
g_entity_t *G_ModeItemDropCallback(g_client_t *cl, const g_item_t *item);
void G_ModeItemResetDropped(g_entity_t *ent);
bool G_ModeItemReset(g_entity_t *ent);
g_team_t *G_ModeTeamForFlag(const g_entity_t *ent);
g_entity_t *G_ModeFlagForTeam(const g_team_t *team);
const g_item_t *G_ModeGetFlag(const g_client_t *cl);
int32_t G_ModeEffectForTeam(const g_team_t *team);
void G_ModeBotDirectives(g_client_t *cl, const g_entity_t *ent,
                         g_mode_bot_directives_t *directives);
void G_ModeBotTarget(const g_client_t *cl, const g_entity_t *target,
                    float *priority, float *chase_multiplier);
bool G_ModeBotCanPickup(const g_client_t *cl, const g_entity_t *item,
                        bool *can_pickup);
void G_ModeModifyDamage(g_damage_t *damage, bool *cancel);
uint32_t G_ModeModifyWeaponInterval(g_client_t *cl, uint32_t interval);
void G_ModeDamageApplied(const g_damage_t *damage, int32_t damage_health,
                         bool was_dead);
bool G_ModeRespawn(g_client_t *cl, bool voluntary);
g_entity_t *G_ModeSelectSpawn(g_client_t *cl);
bool G_ModeAssignTeam(g_client_t *cl);
bool G_ModeCheckRules(void);
bool G_ModeResetLegacyFlagItem(g_mode_t *mode, g_entity_t *ent);
bool G_ModeHasCapability(uint32_t capability);
bool G_ModeTeamplay(void);
bool G_ModeObjectiveEnabled(void);
bool G_ModeObjectiveCvarChanged(int32_t *enabled);
int32_t G_ModeCaptureLimit(void);
bool G_ModeCaptureLimitCvarChanged(int32_t *limit);
bool G_CheckMatchLimit(void);
const char *G_ModePrimaryName(bool objective);
const char *G_ModeModifierName(g_gameplay_t gameplay);
g_gameplay_t G_ModeGameplayByName(const char *name);

/**
 * @brief Notify the active mode of entity slot lifecycle changes.
 */
void G_ModeEntitySpawn(g_entity_t *ent);
void G_ModeEntityFree(g_entity_t *ent);
bool G_ModeSpawnEntityClass(g_entity_t *ent);
const g_item_t *G_ModeFindItemByClassName(const char *classname);
const g_item_t *G_ModeFindItem(const char *name);
const g_item_t *G_ModeItemByTag(g_item_tag_t tag);
size_t G_ModeItemCount(g_item_type_t type);
const g_item_t *G_ModeItemAt(g_item_type_t type, size_t index);
void G_ModePublishItemCatalog(void);

/**
 * @brief Accessors for mode-private instance and AoS component data.
 */
g_mode_t *G_ModeActive(void);
/** @brief Return an active modifier instance by stable name, if present. */
g_mode_t *G_ModeModifier(const char *name);
/**
 * @brief Return the active mode or modifier that owns a dynamic item.
 *
 * Mode-owned dynamic item callbacks do not receive a mode argument. This
 * lookup lets them recover their owner without depending on whether it is a
 * primary mode or a modifier, or on its position in the composition.
 */
g_mode_t *G_ModeForItem(const g_item_t *item);
const g_mode_context_t *G_ModeContext(const g_mode_t *mode);
/** @brief Return the single mode-wide state record for this instance. */
void *G_ModeState(g_mode_t *mode);
void *G_ModeEntityData(g_mode_t *mode, int32_t entity_num);
void *G_ModeClientData(g_mode_t *mode, int32_t client_num);

/** @brief Tech modifier services retained for legacy common call sites. */
bool G_ModeTechsEnabled(void);
const g_item_t *G_ModeGetTech(const g_client_t *cl);
bool G_ModeHasTech(const g_client_t *cl, g_item_tag_t tech);
g_entity_t *G_ModeTossTech(g_client_t *cl);
void G_ModeResetDroppedTech(g_entity_t *ent);
g_entity_t *G_ModeSelectTechSpawnPoint(void);
bool G_ModePickupTech(g_client_t *cl, g_entity_t *ent);
g_entity_t *G_ModeDropTech(g_client_t *cl, const g_item_t *item);
void G_ModePlayTechSound(g_client_t *cl);

#endif /* __GAME_LOCAL_H__ */
