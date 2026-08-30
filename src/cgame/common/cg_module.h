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

#include "cg_types.h"
#include "bg_vote.h"

#if defined(__CG_LOCAL_H__)

/**
 * @brief The contract every client game module implements for the common sources,
 * and the single authoritative list of the client's variation points.
 *
 * @details This is the client half of `g_module.h`, and it works the same way:
 * common code calls these, each module defines them, and a variation point that
 * several optional features may each want a say in is a chainable hook rather
 * than a named function. Common holds the default behaviour as a `_Common` tail
 * in the domain file that calls it; a feature installs itself over the top in its
 * own `_Init`, keeping the value it displaced to call as `previous`.
 *
 * A hook MUST be installed from `Cg_Init`, behind a `static bool installed`, and
 * never from anything per-level or per-connection. `Cg_Init` runs more than once
 * per process: the client calls it at startup, when the `game` cvar changes, when
 * a server reports a different game directory, and on `r_restart` - which
 * re-initializes the *same* module image. `dlclose` does not unload the client
 * game on macOS, so the file statics survive all four, and installing a second
 * time would set `previous.X` to this feature's own function and spin the first call
 * forever.
 *
 * @remarks A guard MUST name a feature - `G_CTF`, `G_HOOK`, `G_TECH` - and never
 * a module, and the client game MUST be built with the same feature defines as
 * its game module. The two sides share that module's `g_types.h`, so a define set
 * that differs between them shifts the wire layout and presents as a network
 * fault rather than a build failure.
 */

/**
 * @defgroup module-contract Module contract
 * @brief What every client game module defines for itself. A missing definition is
 * a link error, which is how a new module learns what it owes.
 * @{
 */

/**
 * @brief The module's own initialization, called from `Cg_Init` after the features
 * common ships have installed themselves.
 * @details This is where a module installs the hooks for behaviour that is its own
 * rather than one of common's features. It exists because a mod Quetoo does not
 * ship can only add files to its own directory: it cannot add a call to `Cg_Init`,
 * and a guard named after it could never be committed here, so without this seam
 * its only way to install a hook would be to fork `cg_main.c`.
 *
 * Every module MUST define it, even if the body is empty. Being called last means
 * a module's hooks sit at the head of every chain, so they may wrap a shipped
 * feature rather than only precede the tail.
 */
void Cg_Module_Init(void);

/**
 * @brief The module's own shutdown, called from `Cg_Shutdown` before the client
 * game's memory tags are freed, so that a module may still touch what it
 * allocated.
 * @details Every module MUST define it, even if the body is empty.
 *
 * This is for what a module allocated or opened, and **MUST NOT uninstall
 * hooks**. `Cg_Init` and `Cg_Shutdown` run on every client game initialization,
 * while a hook installs exactly once per module image, behind a `static bool
 * installed` - so uninstalling here would tear a link out of a chain that the next
 * `Cg_Init` declines to rebuild. Chains are not unwound; they are built once and
 * left.
 */
void Cg_Module_Shutdown(void);

/**
 * @}
 * @defgroup cg-hooks-hud Heads up display
 * @brief What the HUD is made of, and how it is arranged. Tail in cg_hud.c.
 * @{
 */

/**
 * @brief Draws the elements of the HUD that hold a position, arranging them.
 * @param layout The running position of each column that stacks, which an
 * implementation advances by whatever it drew.
 * @details One hook for the whole arrangement rather than one per element,
 * because the elements are independent draws with nothing to return, and a mod
 * with an opinion about the HUD has an opinion about all of it. The blocks it
 * arranges - `Cg_DrawFrags`, `Cg_DrawPowerups` and the rest - are public in
 * `cg_hud.h` for exactly that reason.
 *
 * Chainable, and a feature adding an element MUST call previous first and draw
 * after it, so that what it draws lands below what it did not write. A module
 * that arranges the whole HUD itself does not defer to previous at all, and then
 * owns every element: nothing it declines to call gets drawn.
 *
 * What this does not decide is the framing - the `cg_draw_hud` cvar, the
 * intermission, the crosshair, the editor, the clock beneath the stat column, the
 * vote in progress, and the overlays that place themselves. `Cg_DrawHud` keeps those, so that a module
 * cannot lose the damage blend or the hit sound by forgetting to draw them. A
 * module that wants the clock somewhere else overrides `cg_hud.c` outright, which
 * vpath has always allowed.
 */
typedef void (*DrawHudElements)(const player_state_t *ps, cg_hud_layout_t *layout);

extern DrawHudElements Cg_DrawHudElements;

/**
 * @}
 * @defgroup cg-hooks-gameplay Gameplay
 * @brief What modes this module offers in the create-server menu. Tail in cg_main.c.
 * @{
 */

/**
 * @brief Returns the gameplay modes this module's server actually supports, for
 * the create-server menu's gameplay Select.
 * @param count The number of modes returned.
 * @return The modes, `GAMEPLAY_TEAMS` included where a mode is team play.
 * @details A single owner, like `G_ClampGameplay` on the game side: a module
 * that plays exactly one mode replaces this outright rather than adding to the
 * list common offers. The menu MUST NOT assume a fixed set - a mod that plays
 * only one mode should not have to hide options it will never honor.
 */
typedef const g_gameplay_t *(*ListGameplayModes)(size_t *count);

extern ListGameplayModes Cg_ListGameplayModes;

/**
 * @}
 * @defgroup cg-hooks-movement Movement
 * @brief How the client predicts movement. Tails in cg_predict.c.
 * @{
 */

/**
 * @brief Decides whether `ent` clips a trace made on behalf of `mover`, after the
 * client has applied its own skip rules. There is no tail: the chain is `NULL`
 * until a feature installs a link, and the client does not call an empty one.
 * @details Chainable, and the reciprocal of the game's `ClipEntity`: a feature
 * installs the same rule on both sides, or prediction disagrees with the server.
 * Prediction traces on behalf of `cgi.client->entity`; `mover` is `NULL` for a
 * trace with no entity behind it, as it is for `cgi.Clip`. An implementation
 * MUST be pure.
 */
typedef bool (*ClipEntity)(const cl_entity_t *mover, const cl_entity_t *ent, const vec3_t start, const vec3_t end, const box3_t bounds);

extern ClipEntity Cg_ClipEntity;

/**
 * @brief Whether the client predicts its own movement this frame. The default
 * says no for demos, the third person view, a dead or frozen player, a frame
 * with nothing to delta from, or when `cg_predict` is off.
 * @details Chainable. A feature that cannot vouch for its prediction - one whose
 * physics the server has not yet confirmed, say - answers false and otherwise
 * defers to previous.
 * @return True to predict.
 */
typedef bool (*UsePrediction)(void);

extern UsePrediction Cg_UsePrediction;

/**
 * @brief Builds the movement command from the client's input: the movement,
 * the buttons, and the muzzle for an attack. The default reads the bound keys.
 * @details Chainable. A feature adding an input reads its own key, sets what it
 * sets on `cmd` and defers to previous.
 */
typedef void (*Move)(pm_cmd_t *cmd);

extern Move Cg_Move;

/**
 * @brief A pending command with time is about to be run through `Pm_Move` for
 * prediction. The move is set up once from the last server frame and carried
 * through every pending command, so whatever a feature sets on it here stays
 * set for the commands that follow.
 * @details Notification; the tail does nothing.
 */
typedef void (*MoveCommandWillRun)(pm_move_t *pm, const cl_cmd_t *cmd);

extern MoveCommandWillRun Cg_MoveCommandWillRun;

/**
 * @brief A pending command with time has been run through `Pm_Move` for
 * prediction.
 * @details Notification; the tail does nothing.
 */
typedef void (*MoveCommandDidRun)(const pm_move_t *pm, const cl_cmd_t *cmd);

extern MoveCommandDidRun Cg_MoveCommandDidRun;

/**
 * @brief Every pending command has been predicted and `pm` holds the result
 * the view will be rendered from.
 * @details Notification; the tail does nothing.
 */
typedef void (*PredictionDidComplete)(const pm_move_t *pm);

extern PredictionDidComplete Cg_PredictionDidComplete;

/**
 * @}
 * @defgroup cg-hooks-messages Messages
 * @brief What the server sends. Tails in cg_main.c.
 * @{
 */

/**
 * @brief Parses a server command, ahead of the built-in ones. A module numbers
 * its commands from `SV_CMD_CGAME` in its `g_types.h`.
 * @details Chainable. A feature reads the payload of the commands it owns and
 * answers true; it defers to previous for the rest. The tail parses nothing, so
 * an unclaimed command falls through to the built-in ones. A feature that
 * answers true for a built-in command replaces its parsing entirely. A feature
 * MUST read exactly the payload its game half wrote, or every message after it
 * is misparsed.
 * @return True if the command was parsed.
 */
typedef bool (*ParseServerCommand)(int32_t cmd);

extern ParseServerCommand Cg_ParseServerCommand;

/**
 * @brief Applies a config string that changed, ahead of the built-in handling.
 * A module's config strings start at `CS_GAME` in its `g_types.h`.
 * @details Chainable. A feature applies the indices it owns and answers true;
 * it defers to previous for the rest. The tail applies nothing.
 * @return True if the config string was applied.
 */
typedef bool (*ParseConfigString)(int32_t index);

extern ParseConfigString Cg_ParseConfigString;

/**
 * @}
 * @defgroup cg-hooks-lifecycle Lifecycle
 * @brief The client game's life across connections and frames. Tails in
 * cg_main.c and cg_media.c.
 *
 * These are notifications: the module is told what happened and MUST NOT try
 * to change it.
 * @{
 */

/**
 * @brief The client game state was cleared, on connecting to a server or
 * leaving one. A feature drops what it knew about the last server here.
 * @details Notification; the tail does nothing.
 */
typedef void (*StateDidClear)(void);

extern StateDidClear Cg_StateDidClear;

/**
 * @brief The level's media are loaded. A feature loads its own here.
 * @details Notification; the tail does nothing.
 */
typedef void (*MediaDidLoad)(void);

extern MediaDidLoad Cg_MediaDidLoad;

/**
 * @brief The scene holds every entity, effect, flare, sprite and light the
 * client game adds for this frame. A feature adds its own here.
 * @details Notification; the tail does nothing.
 */
typedef void (*SceneDidPopulate)(const cl_frame_t *frame);

extern SceneDidPopulate Cg_SceneDidPopulate;

/**
 * @brief The HUD, the scoreboard and the editor are drawn for this frame. A
 * feature that draws an overlay of its own does so here, over the top.
 * @details Notification; the tail does nothing.
 */
typedef void (*ScreenDidUpdate)(const cl_frame_t *frame);

extern ScreenDidUpdate Cg_ScreenDidUpdate;

/**
 * @}
 * @defgroup cg-hooks-entities Entities
 * @brief The entities the server sends. Tails in cg_entity.c.
 * @{
 */

/**
 * @brief Whether an entity the server sent is shown at all: interpolated, given
 * its sounds and events, trailed and added to the scene. The default shows
 * everything.
 * @details Chainable. A feature that hides other players, say, answers false
 * for them and defers to previous for the rest.
 * @return True to show the entity.
 */
typedef bool (*FilterEntity)(const cl_entity_t *ent);

extern FilterEntity Cg_FilterEntity;

/**
 * @brief Augments the renderer entity for an entity the server sent, from its
 * effects: rotation, bobbing, lights, the shells. The default maps the effects
 * common knows.
 * @details Chainable. A feature that defines an effect of its own calls previous
 * and then reads its flag, as `EF_GAME` leaves it room to.
 */
typedef void (*EntityEffects)(cl_entity_t *ent, r_entity_t *e);

extern EntityEffects Cg_EntityEffects;

/**
 * @brief The client info that dresses an entity wearing a player model: its
 * models, skins and colors. The default is the info of the client slot the
 * entity names.
 * @details Chainable. A feature whose entity is a player that is not a client -
 * a ghost, a dummy - answers its own info for it and defers to previous for
 * the rest.
 */
struct cg_client_info_s;
typedef struct cg_client_info_s *(*ClientInfo)(const cl_entity_t *ent);

extern ClientInfo Cg_ClientInfo;

/**
 * @}
 * @defgroup cg-hooks-presentation Presentation
 * @brief What the client game says about the game. Tails in cg_discord.c and
 * cg_score.c.
 * @{
 */

/**
 * @brief Describes the game being played, for Discord and the like: "Arena",
 * "2-Team CTF".
 * @details Chainable so that a feature can qualify what it was handed, but a
 * feature that names its mode outright does not defer to previous.
 * @return A static or `va` string.
 */
typedef const char *(*DescribeGameMode)(void);

extern DescribeGameMode Cg_DescribeGameMode;

/**
 * @brief Draws the scoreboard when the player state asks for it.
 * @details Not chained: exactly one arrangement is possible. A module with its
 * own board installs over the top and does not call previous; the parsed scores
 * are still assembled by common, so it only replaces the drawing.
 */
typedef void (*DrawScores)(const player_state_t *ps);

extern DrawScores Cg_DrawScores;

/**
 * @brief The kinds of vote the Vote screen offers. The default is the common
 * list in `bg_vote.h`.
 * @details Not chained: a module with votes of its own returns a list holding
 * the common ones and its own, so that the screen shows everything the game
 * accepts, and the game's `PrepareVote` chain must accept every name listed.
 * The list MUST be static storage; the screen keeps pointers into it.
 */
typedef const vote_type_t *(*ListVoteTypes)(size_t *count);

extern ListVoteTypes Cg_ListVoteTypes;

/**
 * @}
 */
#endif
