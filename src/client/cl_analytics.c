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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <time.h>

#include <Objectively/JSONContext.h>
#include <Objectively/JSONSerializers.h>
#include <Objectively/RESTClient.h>

#include "cl_local.h"
#include "shared/md5.h"

Cvar *cl_analyticsUrl;

/**
 * @brief Returns true if `cl_analyticsUrl` names an endpoint to report to.
 * @remarks `0` disables analytics, and the empty string does too. The empty string cannot actually
 * be typed: `Cvar_Set_f` wants three tokens and an empty quoted argument does not produce one, so
 * `set cl_analyticsUrl ""` silently does nothing. `0` is what we document.
 */
static bool Cl_AnalyticsEnabled(void) {
  return cl_analyticsUrl->string[0] && q_strcmp(cl_analyticsUrl->string, "0");
}

/**
 * @brief The session start payload. Field names are the JSON keys.
 */
typedef struct {
  char event[8];
  char token[33];
  char sessionId[37];
  char version[64];
  char build[64];
  char buildNumber[32];
  char platform[32];
  char device[128];
  char vendor[64];
  char renderer[32];
  int32_t cpuCores;
  int32_t systemRamMb;
} ClientAnalyticsStart;

/**
 * @brief The session end payload. Field names are the JSON keys.
 */
typedef struct {
  char event[8];
  char token[33];
  char sessionId[37];
  int32_t duration;
  int32_t maps;
} ClientAnalyticsEnd;

static struct {
  bool enabled;
  char url[MAX_STRING_CHARS];
  char token[33];
  char sessionId[37];
  uint32_t startTicks;
  int32_t maps;
} module;

/**
 * @brief `RESTClientCompletion` for the session start request.
 */
static void Cl_AnalyticsCallback(int32_t status, Data *data, void *userData) {

  if (status < 200 || status >= 300) {
    Com_Debug(DEBUG_CLIENT, "POST to %s failed (HTTP %d)\n", (const char *) userData, status);
  }
}

/**
 * @brief Derives the daily analytics token from the client `guid`.
 * @details The token is `md5(guid + UTC date)`, so that the raw `guid` never leaves this machine
 * and the value rotates every day. Callers MUST NOT substitute a stronger digest: md5's weakness
 * is collision resistance, which is meaningless for a pseudonym, and the input is a 122 bit random
 * UUID, so the digest cannot be walked back to a `guid` regardless.
 * @remarks The date is UTC rather than local, so that the day boundary is the same worldwide.
 */
static void Cl_AnalyticsToken(void) {

  const time_t now = time(NULL);

  char date[11];
  strftime(date, sizeof(date), "%Y-%m-%d", gmtime(&now));

  md5_ctx ctx;
  md5_init(&ctx);
  md5_update(&ctx, guid->string, q_strlen(guid->string));
  md5_update(&ctx, date, q_strlen(date));

  uint8_t digest[16];
  md5_finalize(&ctx, digest);

  for (size_t i = 0; i < lengthof(digest); i++) {
    q_snprintf(module.token + i * 2, 3, "%02x", digest[i]);
  }
}

/**
 * @brief Serializes and POSTs one analytics payload to `cl_analyticsUrl`.
 * @param start True for the session start payload, false for the session end payload.
 * @remarks The end payload blocks, because the process exits immediately after this returns and an
 * asynchronous request would die in flight.
 */
static void Cl_PostAnalytics(bool start) {

  JSONContext *ctx = $(alloc(JSONContext), init);
  Data *data;

  if (start) {

    const JSONProperties properties = MakeJSONProperties(ClientAnalyticsStart,
      MakeJSONProperty(ClientAnalyticsStart, event,       JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, token,       JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, sessionId,   JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, version,     JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, build,       JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, buildNumber, JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, platform,    JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, device,      JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, vendor,      JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, renderer,    JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, cpuCores,    JSONSerializeInt32,      NULL, NULL),
      MakeJSONProperty(ClientAnalyticsStart, systemRamMb, JSONSerializeInt32,      NULL, NULL)
    );

    ClientAnalyticsStart payload = {
      .cpuCores = SDL_GetNumLogicalCPUCores(),
      .systemRamMb = SDL_GetSystemRAM()
    };

    q_strlcpy(payload.event, "start", sizeof(payload.event));
    q_strlcpy(payload.token, module.token, sizeof(payload.token));
    q_strlcpy(payload.sessionId, module.sessionId, sizeof(payload.sessionId));
    q_strlcpy(payload.version, VERSION, sizeof(payload.version));
    q_strlcpy(payload.build, BUILD, sizeof(payload.build));
    q_strlcpy(payload.buildNumber, BUILD_NUMBER, sizeof(payload.buildNumber));
    q_strlcpy(payload.platform, SDL_GetPlatform(), sizeof(payload.platform));
    q_strlcpy(payload.device, rConfig.device, sizeof(payload.device));
    q_strlcpy(payload.vendor, rConfig.vendor, sizeof(payload.vendor));
    q_strlcpy(payload.renderer, rConfig.renderer, sizeof(payload.renderer));

    data = $(ctx, dataFromStruct, &properties, &payload);
  } else {

    const JSONProperties properties = MakeJSONProperties(ClientAnalyticsEnd,
      MakeJSONProperty(ClientAnalyticsEnd, event,     JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsEnd, token,     JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsEnd, sessionId, JSONSerializeCharacters, NULL, NULL),
      MakeJSONProperty(ClientAnalyticsEnd, duration,  JSONSerializeInt32,      NULL, NULL),
      MakeJSONProperty(ClientAnalyticsEnd, maps,      JSONSerializeInt32,      NULL, NULL)
    );

    ClientAnalyticsEnd payload = {
      .duration = (quetoo.ticks - module.startTicks) / 1000,
      .maps = module.maps
    };

    q_strlcpy(payload.event, "end", sizeof(payload.event));
    q_strlcpy(payload.token, module.token, sizeof(payload.token));
    q_strlcpy(payload.sessionId, module.sessionId, sizeof(payload.sessionId));

    data = $(ctx, dataFromStruct, &properties, &payload);
  }

  release(ctx);
  assert(data);

  if (start) {
    $($$(RESTClient, sharedInstance), postAsync, module.url, data, NULL, Cl_AnalyticsCallback, module.url);
  } else {
    Cl_AnalyticsCallback($($$(RESTClient, sharedInstance), post, module.url, data, NULL, NULL), NULL, module.url);
  }

  release(data);
}

/**
 * @brief Counts one map load towards the current session.
 * @remarks `Cl_LoadMedia` also runs on a renderer or sound restart, which is not a new map. Those
 * two callers only reload while the client is active, so `Cl_LoadMedia` filters on that.
 */
void Cl_AnalyticsMap(void) {

  if (module.enabled) {
    module.maps++;
  }
}

/**
 * @brief Begins an analytics session, and reports the client build and hardware.
 * @remarks This must run after `Cl_InitGuid` and after `R_Init`, because it reads both.
 */
void Cl_InitAnalytics(void) {

  memset(&module, 0, sizeof(module));

  if (!Cl_AnalyticsEnabled()) {
    return;
  }

  module.enabled = true;
  module.startTicks = quetoo.ticks;

  char base[MAX_STRING_CHARS];
  q_strlcpy(base, cl_analyticsUrl->string, sizeof(base));

  for (char *c = base + q_strlen(base) - 1; c >= base && *c == '/'; c--) {
    *c = '\0';
  }

  q_snprintf(module.url, sizeof(module.url), "%s/api/sessions", base);

  Cl_AnalyticsToken();
  Com_Uuid(module.sessionId, sizeof(module.sessionId));

  Cl_PostAnalytics(true);
}

/**
 * @brief Ends the analytics session, reporting its duration and map count.
 * @remarks The cvar is read again here, so that a player who opts out during a session is not
 * reported when they quit. A change to the URL itself is deliberately ignored until the next
 * launch, because this request has to reach whatever received the session start.
 */
void Cl_ShutdownAnalytics(void) {

  if (!module.enabled || !Cl_AnalyticsEnabled()) {
    return;
  }

  Cl_PostAnalytics(false);

  module.enabled = false;
}
