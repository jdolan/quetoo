/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "r_types.h"

#if BUILD_RTX

/**
 * @brief Registers RTX/Vulkan cvars during renderer-local initialization.
 */
void R_Rtx_InitLocal(void);

/**
 * @brief Registers RTX/Vulkan renderer commands.
 */
void R_Rtx_InitCommands(void);

/**
 * @brief Lets the isolated RTX/Vulkan renderer draw or overlay the main view.
 */
void R_Rtx_RenderView(const r_view_t *view);

/**
 * @brief Releases all RTX/Vulkan renderer state.
 */
void R_Rtx_RendererShutdown(void);

#endif
