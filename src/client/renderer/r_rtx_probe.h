/*
 * Copyright(c) 2026 QuetooPlus.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <stdbool.h>

/**
 * @file
 * @brief Native Vulkan ray-tracing capability discovery for the RTX backend.
 *
 * SDL_gpu owns Quetoo's default renderer. The RTX renderer is a separate
 * native Vulkan path because SDL_gpu intentionally does not expose ray-tracing
 * pipeline or acceleration-structure commands.
 */

/**
 * @brief Returns whether an RTX-capable Vulkan physical device is available.
 */
bool R_Rtx_IsSupported(void);

#if defined(__R_LOCAL_H__)
void R_Rtx_Probe_f(void);
#endif
