/*
 * Copyright(c) 2026 QuetooPlus.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "r_rtx_output.h"

#if BUILD_RTX

bool R_Rtx_SceneRender(const r_rtx_device_t *device, r_rtx_output_t *output, const r_view_t *view);
void R_Rtx_SceneShutdown(const r_rtx_device_t *device);

#endif
