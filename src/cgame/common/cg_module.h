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
 * own `_Init`, keeping the previous value to call as super.
 *
 * A hook MUST be installed from `Cg_Init`, behind a `static bool installed`, and
 * never from anything per-level or per-connection. `Cg_Init` runs more than once
 * per process: the client calls it at startup, when the `game` cvar changes, when
 * a server reports a different game directory, and on `r_restart` - which
 * re-initializes the *same* module image. `dlclose` does not unload the client
 * game on macOS, so the file statics survive all four, and installing a second
 * time would set `super.X` to this feature's own function and spin the first call
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
 */
#endif /* __CG_LOCAL_H__ */
