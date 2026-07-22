/*
 * Copyright(c) 2026 QuetooPlus.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "common/common.h"
#include "r_rtx_device.h"
#include "r_rtx_probe.h"

#if BUILD_RTX

bool R_Rtx_IsSupported(void) {
  r_rtx_device_t device;
  if (!R_Rtx_DeviceInit(&device)) {
    return false;
  }

  const bool supported = R_Rtx_DeviceSmokeTest(&device);
  R_Rtx_DeviceShutdown(&device);
  return supported;
}

void R_Rtx_Probe_f(void) {
  const bool supported = R_Rtx_IsSupported();
  Com_Print("Native Vulkan RTX support: ^%d%s^7\n", supported ? 2 : 1,
            supported ? "available" : "unavailable");
}

#else

bool R_Rtx_IsSupported(void) {
  return false;
}

#endif
