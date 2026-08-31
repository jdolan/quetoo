/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#pragma once

#include "g_types.h"
#include "bg_pmove.h"

#if defined(__G_LOCAL_H__)

/**
 * @brief The contract every game module implements for the common sources.
 *
 * @details Common code calls these; each module defines them. This is
 * deliberately not a set of #if guards: a guard would put knowledge of every
 * module that will ever exist into shared code, whereas a module implementing a
 * named function keeps that knowledge where it belongs.
 *
 * Variation points that several optional features may each want a say in are
 * chainable hooks rather than named functions. Common holds the default
 * behaviour; a feature installs itself over the top in its own `_Init`, keeping
 * the value it displaced to call as `previous`. Composition then falls out: a
 * module with flags and techs gets both, one with only techs gets only that, and neither
 * feature has to know the other exists.
 *
 * Hooks MUST be installed from `G_Init`, once per module load. Installing from
 * anything per-level would grow the chain on every map restart. Chain order is
 * installation order, so the order of the `_Init` calls in a module's `G_Init`
 * is part of its behaviour.
 */

/**
 * @defgroup module-contract Module contract
 * @brief What every module defines for itself. A missing definition is a link error,
 * which is how a new module learns what it owes.
 * @{
 */

/**
 * @brief The module's own initialization, called once per module load from
 * `G_Init`, after the features common ships have installed themselves.
 * @details This is where a module installs the hooks for behaviour that is its
 * own rather than one of common's features. It exists because a mod Quetoo does
 * not ship can only add files to its own directory: it cannot add a call to
 * `G_Init`, and a guard named after it could never be committed here, so without
 * this seam its only way to install a hook would be to fork `g_main.c`.
 *
 * Every module MUST define it, even if the body is empty. A missing definition
 * is a link error, which is the right way to tell a new module what it owes.
 * Installing from here rather than from `G_Init` also means a module's hooks sit
 * at the head of every chain, so they may wrap a shipped feature.
 */
void G_Module_Init(void);

/**
 * @brief The module's own shutdown, called from `G_Shutdown` before the game's
 * memory tags are freed, so that a module may still touch what it allocated.
 * @details Every module MUST define it, even if the body is empty.
 *
 * This is for what a module allocated or opened, and **MUST NOT uninstall
 * hooks**. `G_Init` and `G_Shutdown` run on every server initialization, while a
 * hook installs exactly once per module image, behind a `static bool installed` -
 * so uninstalling here would tear a link out of a chain that the next `G_Init`
 * declines to rebuild. Chains are not unwound; they are built once and left.
 */
void G_Module_Shutdown(void);

/**
 * @}
 * @defgroup hooks-level Level
 * @brief Setting the level up, and resolving what a feature holds for it. Tails in
 * g_entity.c.
 * @{
 */

/**
 * @brief Resolves the settings a feature holds for the level ahead, after the
 * worldspawn has been read, and again whenever the game restarts.
 * @details Nothing to do by default. A feature decides here whether it is
 * enabled this level, and publishes any config strings the client needs.
 */
typedef void (*ConfigureLevel)(void);

extern ConfigureLevel G_ConfigureLevel;

/**
 * @brief Indexes the models and sounds the module needs for the level ahead.
 * @details The whole of the deathmatch media is the default; a feature indexes
 * its own on top, and keeps the indices itself rather than growing `g_media`.
 */
typedef void (*InitMedia)(void);

extern InitMedia G_InitMedia;

/**
 * @brief The level is about to be spawned: `g_level` is reset and named, the
 * previous level's entities are about to be freed, and nothing of the new one,
 * not even the worldspawn, exists yet. A feature that layers per-level
 * settings over cvars the worldspawn reads does so here.
 * @details Notification; the tail does nothing.
 */
typedef void (*LevelWillSpawn)(void);

extern LevelWillSpawn G_LevelWillSpawn;

/**
 * @brief Initializes a freshly spawned entity by its class name. The default
 * knows the items, the weapons' projectiles and the built-in classes.
 * @details Chainable. A feature adding entities initializes the class names it
 * owns and defers to previous for the rest. An entity nobody claims is warned
 * about and freed by `G_SpawnEntity`. The editor spawns its inert placeholders
 * without consulting the chain.
 * @return True if the entity was initialized.
 */
typedef bool (*InitEntity)(g_entity_t *ent);

extern InitEntity G_InitEntity;

/**
 * @}
 * @defgroup hooks-items Items
 * @brief Placing items in the world, and what becomes of them. Tails in g_item.c.
 * @{
 */

/**
 * @brief Decides whether the current gameplay withholds an item from the level
 * altogether.
 * @details Arena and instagib withhold everything by default. A feature whose
 * items are the point of the level, such as the flags, exempts its own.
 */
typedef bool (*InhibitItem)(const g_entity_t *ent);

extern InhibitItem G_InhibitItem;

/**
 * @brief Fills in an item's behaviour - how it is picked up, and how it is
 * dropped - from its type.
 * @details The default answers for the deathmatch item types and errors on any
 * type it does not recognise, so a feature bringing its own item type MUST
 * install ahead of it and answer for that type rather than deferring.
 */
typedef void (*InitItem)(g_item_t *it);

extern InitItem G_InitItem;

/**
 * @brief Disposes of a dropped item that has left the world - fallen into the
 * void, or been caught by a hurt trigger or an explosion.
 * @details A plain deathmatch module frees it. A module with flags returns them
 * to their base instead, and one with techs respawns them.
 */
typedef void (*ResetDroppedItem)(g_entity_t *ent);

extern ResetDroppedItem G_ResetDroppedItem;

/**
 * @brief Places an item in the world for the start of a level, deciding whether
 * it can be seen and touched.
 * @details The whole of the deathmatch placement is the default, and a feature
 * that has more to say about its own items installs over the top. Anything it
 * changes after deferring to previous MUST be linked again, because previous links.
 */
typedef void (*ResetItem)(g_entity_t *ent);

extern ResetItem G_ResetItem;

/**
 * @}
 * @defgroup hooks-inventory Inventory
 * @brief What a client carries: given at the spawn, parted from on death or
 * disconnect. Tails in g_item.c, and in g_client.c for the spawn.
 * @{
 */

/**
 * @brief Gives a client their starting inventory when they spawn.
 * @details The default is the machinegun with its ammo, or the item set's
 * equivalent. Chainable: a feature that adds to the loadout calls previous and
 * then gives; a mode with no weapons at all does not defer to previous. The
 * tail lives in g_client.c, beside the spawn.
 */
typedef void (*InitInventory)(g_client_t *cl);

extern InitInventory G_InitInventory;

/**
 * @brief Resolves the item a client named to one they are carrying, or `NULL`
 * when the name means nothing here.
 * @details Deathmatch resolves the name against the item list. Features that
 * answer to a category rather than an item name, such as the "flag" a client
 * happens to be carrying, return that item outright.
 *
 * The chain resolves to an item rather than to another name because a name can
 * not say *which* item is meant: all four flags are called "Enemy Flag", so
 * handing a name back to the item list finds the first of them and not the one
 * being carried.
 */
typedef const g_item_t *(*ResolveInventoryItem)(g_client_t *cl, const char *name);

extern ResolveInventoryItem G_ResolveInventoryItem;

/**
 * @brief Tosses whatever the client is carrying that must not leave play with
 * them, when they change team or become a spectator.
 * @details The quad damage powerup is the default. Flags, techs and the grapple
 * each add their own, which is why this is a chain rather than a list of calls
 * in whichever command happens to need it.
 */
typedef void (*TossInventory)(g_client_t *cl);

extern TossInventory G_TossInventory;

/**
 * @}
 * @defgroup hooks-combat Combat
 * @brief What an attack does. Tails in g_combat.c.
 * @{
 */

/**
 * @brief Scales the damage and the knockback an attack is about to impart, or
 * vetoes the attack outright. Runs once the target is known to take damage and
 * is not under respawn protection, before invulnerability, friendly fire and
 * self damage are resolved, so that a veto precedes every side effect of being
 * hit. Respawn protection runs first because it has no side effects of its own,
 * and a hook that ran ahead of it would fire its own for a hit that never lands.
 * @details The quad damage powerup is the default. A feature carrying its own
 * modifiers, such as the resist and strength techs, installs over the top;
 * because the modifiers multiply, where it calls previous decides only how the
 * integer truncation falls, and whether its own side effects precede a veto
 * further down the chain. A feature that forbids an attack entirely, one in
 * which players never hurt each other, say, returns false without calling
 * previous, and `G_Damage` does nothing at all: no pain, no blood, no knockback.
 * A link that calls previous MUST return false when previous does. `attacker`
 * is the world entity when the attack had none.
 * @return False to abort the attack.
 */
typedef bool (*ModifyDamage)(g_entity_t *target, g_entity_t *attacker, int32_t *damage, int32_t *knockback);

extern ModifyDamage G_ModifyDamage;

/**
 * @}
 * @defgroup hooks-movement Movement
 * @brief How a client moves. Tails in g_client.c.
 * @{
 */

/**
 * @brief Seeds the player move with the state it should start from, before the
 * client's command is applied.
 * @details The default hands the move the entity's own velocity. A feature that
 * takes the movement over, as the grapple does while pulling, sets the move type
 * and the velocity it wants and does not defer to previous.
 */
typedef void (*PrepareMove)(g_client_t *cl, pm_move_t *pm);

extern PrepareMove G_PrepareMove;

/**
 * @brief Decides whether `ent` clips a trace made on behalf of `mover`, after the
 * server has applied its own skip rules. There is no tail: the chain is `NULL`
 * until a feature installs a link, and the server does not call an empty one.
 * @details Chainable; a link whose previous is `NULL` treats it as true. A
 * feature that makes an entity solid to some movers and not others, such as a
 * one-way wall, returns false for the movers it lets through and defers to
 * previous otherwise. `mover` is `NULL` for a trace with no entity behind it.
 * An implementation MUST be pure: the server and the client both call it, from
 * `Sv_Trace` and `Cl_Trace`, and speculative traces such as the bots' lookahead
 * run it many times for a move that never happens.
 */
typedef bool (*ClipEntity)(const g_entity_t *mover, const g_entity_t *ent);

extern ClipEntity G_ClipEntity;

#if defined(G_HOOK)
/**
 * @brief Whether a client may use the grapple hook right now. The default says
 * yes whenever the movement they are running has a hook; a client refused here
 * has their hook detached.
 * @details Chainable. A mode that allows the hook only some of the time answers
 * for its cases and defers to previous.
 * @return True if the client may hook.
 */
typedef bool (*AllowHook)(const g_client_t *cl);

extern AllowHook G_AllowHook;
#endif

/**
 * @}
 * @defgroup hooks-rules Rules
 * @brief The rules a module enforces, once per frame or once per level. Tails in
 * g_rules.c.
 * @{
 */

/**
 * @brief Applies whichever of a feature's own cvars have been modified, once
 * per server frame.
 * @return True if the change requires the level to restart.
 * @details A feature announces the change itself, so that the module enforcing
 * the rules does not have to know which cvars exist. Every implementation MUST
 * clear the `modified` flag of each cvar it consumes, or it will announce the
 * same change on every frame.
 */
typedef bool (*CheckCvars)(void);

extern CheckCvars G_CheckCvars;

/**
 * @brief Decides whether the level has been won, once per server frame.
 * @details A single owner: frag limit and capture limit are answers to the same
 * question, not additions to each other, so a module that plays for captures
 * replaces this rather than chaining onto it. Announce the reason before
 * returning true; the caller only ends the level.
 */
typedef bool (*CheckWinner)(void);

extern CheckWinner G_CheckWinner;

/**
 * @brief Whether the level may advance now that the intermission has run its
 * course. The default says yes.
 * @details Chainable. A feature that holds the level for a vote answers false
 * until the vote is in, then defers to previous.
 * @return True to advance to the next map.
 */
typedef bool (*AllowNextMap)(void);

extern AllowNextMap G_AllowNextMap;

/**
 * @brief Validates a vote a client is calling, writing the argument in the
 * form the vote will be applied and announced with. The default knows the
 * votes in `bg_vote.h`.
 * @details Chainable. A feature with votes of its own validates the types it
 * owns and defers to previous for the rest.
 * @return True if the vote may be called.
 */
typedef bool (*PrepareVote)(const g_client_t *cl, const char *type, const char *arg, char *canonical, size_t size);

extern PrepareVote G_PrepareVote;

/**
 * @brief Applies a vote that passed. The default applies the votes in
 * `bg_vote.h`.
 * @details Chainable. A feature applies the types it owns and defers to
 * previous for the rest.
 * @return True if the vote was applied.
 */
typedef bool (*ApplyVote)(const char *type, const char *arg);

extern ApplyVote G_ApplyVote;

/**
 * @brief Coerces a requested gameplay mode to one this module actually
 * supports, before it is written back to `g_gameplay` or applied to the level.
 * @param gameplay The mode `g_gameplay` parsed to, `GAMEPLAY_TEAMS` included.
 * @return The mode this module will actually play.
 * @details A single owner, like `CheckWinner`: a module that plays exactly one
 * mode replaces this outright rather than qualifying whatever it was handed.
 */
typedef g_gameplay_id_t (*ClampGameplay)(g_gameplay_id_t gameplay);

extern ClampGameplay G_ClampGameplay;

/**
 * @brief Names the gameplay the server is running, for its info strings.
 * @param name The buffer to format into, holding the gameplay name.
 * @param size The size of that buffer.
 * @details Chainable so that a feature can qualify what it was handed, but a
 * feature that renames the gameplay outright does not defer to previous.
 */
typedef void (*FormatGameName)(char *name, size_t size);

extern FormatGameName G_FormatGameName;

/**
 * @}
 * @defgroup hooks-clients Clients
 * @brief A client's life on the server, from connection to disconnection. Tails
 * in g_client.c.
 *
 * Most of these are notifications: the module is told what happened and MUST
 * NOT try to change it. Their names say so - `ClientDidBegin` - where a hook
 * the module decides is named for the decision - `PrepareSpawn`.
 * @{
 */

/**
 * @brief Where and how a client is about to spawn.
 */
typedef struct {

  /**
   * @brief The spawn origin and view angles, from the selected spawn point. The
   * origin is the floor, as a spawn point's is; `PM_STEP_HEIGHT` is added once
   * the chain has run.
   */
  vec3_t origin, angles;

  /**
   * @brief The clip mask the client's entity moves with.
   */
  int32_t clip_mask;

  /**
   * @brief Whether whatever occupies the spawn is telefragged.
   */
  bool kill_box;
} g_client_spawn_t;

/**
 * @brief Adjusts where and how a client spawns, after the spawn point has been
 * selected and before the entity is placed. The default changes nothing.
 * @details Chainable. A feature that spawns players somewhere of its own, or
 * lets them pass through each other, edits `spawn` and calls previous.
 */
typedef void (*PrepareSpawn)(g_client_t *cl, g_client_spawn_t *spawn);

extern PrepareSpawn G_PrepareSpawn;

/**
 * @brief The client is about to enter the game: nothing of their entity exists yet.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientWillBegin)(g_client_t *cl);

extern ClientWillBegin G_ClientWillBegin;

/**
 * @brief The client has entered the game: their entity exists and is placed.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientDidBegin)(g_client_t *cl);

extern ClientDidBegin G_ClientDidBegin;

/**
 * @brief The client's user info is about to be applied: `cl->persistent` still
 * holds what it held, and `user_info` what they sent, unvalidated.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientWillChangeUserInfo)(g_client_t *cl, const char *user_info);

extern ClientWillChangeUserInfo G_ClientWillChangeUserInfo;

/**
 * @brief The client's user info was applied: name, skin, colors and the rest
 * are current on `cl->persistent`.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientDidChangeUserInfo)(g_client_t *cl);

extern ClientDidChangeUserInfo G_ClientDidChangeUserInfo;

/**
 * @brief The client is about to leave: their inventory is still intact, and so
 * is their entity if they ever began, so a feature may still read them.
 * `cl->entity` is `NULL` for a client that connected and left without spawning.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientWillDisconnect)(g_client_t *cl);

extern ClientWillDisconnect G_ClientWillDisconnect;

/**
 * @brief The client has left: their entity is freed and their slot is about to
 * be forgotten.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientDidDisconnect)(g_client_t *cl);

extern ClientDidDisconnect G_ClientDidDisconnect;

/**
 * @brief A movement command has arrived and the buttons are latched, before
 * the client moves or fires. Not called during the intermission, when commands
 * are ignored.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientWillThink)(g_client_t *cl, const pm_cmd_t *cmd);

extern ClientWillThink G_ClientWillThink;

/**
 * @brief The client's entity has moved for this command and is linked, before
 * weapons are handled. Not called for a client chasing another.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientDidMove)(g_client_t *cl, const pm_cmd_t *cmd);

extern ClientDidMove G_ClientDidMove;

/**
 * @brief Handles a command the client sent, ahead of the built-in commands.
 * @details Chainable. A feature that adds commands answers true for the ones it
 * owns and defers to previous for the rest; one that overrides a built-in
 * command answers true for that name and the built-in never sees it. The tail
 * handles nothing, so an unclaimed command falls through to the built-in table
 * and, failing that, to chat. While the level is ending the built-in table
 * ignores everything but chat; `g_level.intermission_time` says so. `cmd` is
 * `gi.Argv(0)`;
 * an implementation MUST NOT call `gi.TokenizeString`, which would replace it
 * under the built-in table that runs next.
 * @return True if the command was handled.
 */
typedef bool (*HandleClientCommand)(g_client_t *cl, const char *cmd);

extern HandleClientCommand G_HandleClientCommand;

/**
 * @brief The client is about to say `text`: their message after variable
 * expansion, without their name, in a buffer of `size` that is theirs to edit.
 * A feature that muzzles returns false, and nothing is delivered or reported;
 * one that filters rewrites `text` in place. Empty, flooding and muted messages
 * never get this far.
 * @details Chainable. A link that calls previous MUST return false when previous
 * does. The tail says yes.
 * @return True to deliver the message.
 */
typedef bool (*ClientWillChat)(g_client_t *cl, char *text, size_t size, bool team);

extern ClientWillChat G_ClientWillChat;

/**
 * @brief The client said something, and it was delivered. `text` is the line as
 * the other clients saw it: the name, its color escapes and a trailing newline,
 * in a buffer that lives only for this call; a feature that keeps it copies it.
 * A muted, empty or flood-limited message is not delivered and not reported.
 * @details Notification; the tail does nothing.
 */
typedef void (*ClientDidChat)(g_client_t *cl, const char *text, bool team);

extern ClientDidChat G_ClientDidChat;

/**
 * @brief Writes a client's stats for this frame, after the built-in ones. A
 * feature that shows something on the HUD writes its stat here, into the slots
 * its module's `g_types.h` numbers. A spectator chasing another client inherits
 * that client's stats wholesale and does not run the chain.
 * @details Chainable; call previous, then write.
 */
typedef void (*WriteStats)(g_client_t *cl);

extern WriteStats G_WriteStats;

/**
 * @brief Writes a client's scoreboard entry, after the built-in fields. A
 * feature with its own columns fills the fields its module's `g_score_t`
 * carries.
 * @details Chainable; call previous, then write.
 */
typedef void (*WriteScore)(const g_client_t *cl, g_score_t *s);

extern WriteScore G_WriteScore;

/**
 * @}
 * @defgroup hooks-frame Frame
 * @brief The server frame. Tail in g_client_view.c.
 * @{
 */

/**
 * @brief The frame is about to run: the level's time has advanced, and nothing
 * has thought yet.
 * @details Notification; the tail does nothing.
 */
typedef void (*FrameWillBegin)(void);

extern FrameWillBegin G_FrameWillBegin;

/**
 * @brief The frame is complete: every entity has run, the rules are checked and
 * every client's player state is built.
 * @details Notification; the tail does nothing.
 */
typedef void (*FrameDidEnd)(void);

extern FrameDidEnd G_FrameDidEnd;

/**
 * @}
 */
#endif
