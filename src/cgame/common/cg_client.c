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

#include "cg_local.h"
#include "game/common/bg_pmove.h"

// team\name\model/skin\shirt\pants\helmet\hue, and the standing box's floor/ceiling
// since 1.0.85; fields beyond these are ignored, so that a newer server's string
// still dresses a player here
#define MIN_CLIENT_INFO_ENTRIES 7
#define MAX_CLIENT_INFO_ENTRIES 8

// where a standing box may plausibly sit and how much it may span, against a garbled field
#define MAX_CLIENT_INFO_EXTENT 128.f
#define MIN_CLIENT_INFO_HEIGHT 16.f

#define DEFAULT_MODEL "enforcer"
#define DEFAULT_SKIN "default"

//                         team name    skin             shirt    pants    helmet   hue
#define DEFAULT_CLIENT_INFO "-1\\newbie\\enforcer/default\\default\\default\\default\\default"

/**
 * @brief Trims leading and trailing whitespace in place.
 */
static char *Cg_StripWhitespace(char *str) {

  while (isspace((unsigned char) *str)) {
    str++;
  }

  if (*str) {
    char *end = str + q_strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end)) {
      *end-- = '\0';
    }
  }

  return str;
}

/**
 * @brief Splits a client info string on backslashes, in place, up to `len` fields.
 */
static size_t Cg_SplitClientInfo(char *str, char **info, size_t len) {

  size_t count = 0;
  char *cursor = str;

  while (true) {
    if (count == len) {
      return count + 1;
    }

    info[count++] = cursor;

    char *separator = q_strchr(cursor, '\\');
    if (separator == NULL) {
      return count;
    }

    *separator = '\0';
    cursor = separator + 1;
  }
}

/**
 * @brief Resolves a single skin line, matching the surface name against
 * all three mesh models and storing the material in the appropriate skins array.
 */
static void Cg_LoadClientSkin(ClientGameClientInfo *ci, char *line) {

  char *skinName, *faceName = line;

  if ((skinName = q_strchr(faceName, ','))) {
    *skinName++ = '\0';

    while (isspace(*skinName)) {
      skinName++;
    }
  } else {
    return;
  }

  if (!*skinName) {
    return;
  }

  const struct {
    const RenderModel *model;
    RenderMaterial **skins;
  } meshes[] = {
    { ci->head, ci->headSkins },
    { ci->torso, ci->torsoSkins },
    { ci->legs, ci->legsSkins },
  };

  for (size_t m = 0; m < 3; m++) {
    if (!meshes[m].model) {
      continue;
    }

    const RenderMeshFace *face = meshes[m].model->mesh->faces;
    for (int32_t i = 0; i < meshes[m].model->mesh->numFaces; i++, face++) {
      if (!q_strcasecmp(faceName, face->name)) {
        meshes[m].skins[i] = cgi.LoadMaterial(skinName, ASSET_CONTEXT_PLAYERS);
        return;
      }
    }
  }
}

/**
 * @brief Parses a .skin file, resolving skins for each face across all three
 * mesh models (head, torso, legs). A face left unresolved is intentionally
 * omitted from that skin variant and is not drawn at all (see `has_skins`
 * and `skins` in RenderEntity, and the checks in r_mesh_draw.c / r_shadow.c);
 * some third-party skins genuinely don't texture certain optional accessory
 * faces (e.g. straps, wrist rockets), and the modeler never intended them to
 * render for that variant. Returns false only if the .skin file itself could
 * not be found/read.
 */
static bool Cg_LoadClientSkins(ClientGameClientInfo *ci, const char *skin) {
  char path[MAX_QPATH], line[MAX_STRING_CHARS];
  char *buffer;
  int64_t len;

  q_snprintf(path, sizeof(path), "players/%s/%s.skin", ci->model, skin);

  if ((len = cgi.LoadFile(path, (void *) &buffer)) == -1) {
    Cg_Debug("%s not found\n", path);
    return false;
  }

  int32_t i = 0, j = 0;
  memset(line, 0, sizeof(line));

  while (i < len) {
    const char c = buffer[i++];
    line[j++] = c;

    if (c == '\n' || c == '\r' || i == len) {
      Cg_LoadClientSkin(ci, Cg_StripWhitespace(line));

      j = 0;
      memset(line, 0, sizeof(line));
    }
  }

  cgi.FreeFile(buffer);

  const struct {
    const RenderModel *model;
    RenderMaterial **skins;
    const char *name;
  } meshes[] = {
    { ci->head, ci->headSkins, "head" },
    { ci->torso, ci->torsoSkins, "torso" },
    { ci->legs, ci->legsSkins, "legs" },
  };

  for (size_t m = 0; m < 3; m++) {
    if (!meshes[m].model) {
      continue;
    }

    const RenderMeshFace *face = meshes[m].model->mesh->faces;
    for (int32_t f = 0; f < meshes[m].model->mesh->numFaces; f++, face++) {
      if (!meshes[m].skins[f]) {
        Cg_Debug("%s: %s %s has no skin, face will not be drawn\n", path, meshes[m].name, face->name);
      }
    }
  }

  return true;
}

/**
 * @brief Ensures that models and skins were resolved for the specified client info.
 */
static bool Cg_ValidateSkin(ClientGameClientInfo *ci) {

  if (!ci->head || !ci->torso || !ci->legs) {
    return false;
  }

  const struct {
    const RenderModel *model;
    RenderMaterial *const *skins;
  } meshes[] = {
    { ci->head, ci->headSkins },
    { ci->torso, ci->torsoSkins },
    { ci->legs, ci->legsSkins },
  };

  // a part with no faces at all (e.g. a head merged into the torso mesh,
  // leaving head.md3 as an empty placeholder) has nothing to validate here
  for (size_t m = 0; m < lengthof(meshes); m++) {
    if (meshes[m].model->mesh->numFaces && !meshes[m].skins[0]) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Resolve and load the specified model/skin for the player.
 */
static bool Cg_LoadClientModel(ClientGameClientInfo *ci, const char *model, const char *skin) {

  q_strlcpy(ci->model, model, sizeof(ci->model));
  q_strlcpy(ci->skin, skin, sizeof(ci->skin));

  char path[MAX_QPATH];

  q_snprintf(path, sizeof(path), "players/%s/head", ci->model);
  ci->head = cgi.LoadModel(path);

  q_snprintf(path, sizeof(path), "players/%s/upper", ci->model);
  ci->torso = cgi.LoadModel(path);

  q_snprintf(path, sizeof(path), "players/%s/lower", ci->model);
  ci->legs = cgi.LoadModel(path);

  if (!ci->head || !ci->torso || !ci->legs) {
    Cg_Debug("Could not load client model %s/%s\n", model, skin);
    return false;
  }

  if (!Cg_LoadClientSkins(ci, ci->skin)) {
    Cg_Debug("Could not load client skins %s/%s\n", model, skin);
    return false;
  }

  q_snprintf(path, sizeof(path), "players/%s/%s_i", ci->model, ci->skin);
  ci->icon = cgi.LoadImage(path, IMG_PIC);

  if (!ci->icon) {
    Cg_Debug("Could not load client icon %s/%s\n", model, skin);
    return false;
  }

  return true;
}

/**
 * @brief Resolves the player name, model and skins for the specified user info string.
 * If validation fails, we fall back on the `DEFAULT_CLIENT_INFO` constant.
 */
void Cg_LoadClient(ClientGameClientInfo *ci, const char *s) {
  const char *t;
  char *v = NULL;
  int32_t i;

  Cg_Debug("%s\n", s);

  // copy the entire string
  q_strlcpy(ci->info, s, sizeof(ci->info));

  i = 0;
  t = s;
  while (*t) { // check for non-printable chars
    if (*t < 32 || *t >= 127) {
      i = -1;
      break;
    }
    t++;
  }

  if (*ci->info == '\0' || i == -1) { // use default
    Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
    return;
  }

  // split info into tokens
  char infoString[sizeof(ci->info)];
  char *info[MAX_CLIENT_INFO_ENTRIES];
  q_strlcpy(infoString, s, sizeof(infoString));

  const size_t entries = Cg_SplitClientInfo(infoString, info, lengthof(info));

  if (entries < MIN_CLIENT_INFO_ENTRIES) { // invalid info
    Cg_LoadClient(ci, DEFAULT_CLIENT_INFO);
  } else {

    // resolve the team
    const GameTeamId teamId = atoi(info[0]);
    if (teamId != TEAM_NONE) {
      ci->team = cgState.teams + teamId;
    } else {
      ci->team = NULL;
    }

    // copy in the name
    q_strlcpy(ci->name, info[1], sizeof(ci->name));

    // check for valid skin
    if ((v = q_strchr(info[2], '/'))) { // it's well-formed
      *v = '\0';

      // load the models
      if (!Cg_LoadClientModel(ci, info[2], v + 1)) {
        if (!Cg_LoadClientModel(ci, info[2], DEFAULT_SKIN)) {
          if (!Cg_LoadClientModel(ci, DEFAULT_MODEL, DEFAULT_SKIN)) {
            Cg_Error("Failed to load default client skin %s/%s\n", DEFAULT_MODEL, DEFAULT_SKIN);
          }
        }
      }
    }

    if (!Color_Parse(info[3], &ci->shirt)) {
      ci->shirt.a = 0;
    }

    if (!Color_Parse(info[4], &ci->pants)) {
      ci->pants.a = 0;
    }

    if (!Color_Parse(info[5], &ci->helmet)) {
      ci->helmet.a = 0;
    }

    const int32_t hue = atoi(info[6]);
    if (hue >= 0) {
      ci->hue = hue;
    } else {
      ci->hue = -1;
    }

    ci->standingFloor = PM_BOUNDS.mins.z;
    ci->standingCeiling = PM_BOUNDS.maxs.z;

    if (entries > MIN_CLIENT_INFO_ENTRIES) {
      float floor, ceiling;
      int32_t n = 0;
      if (sscanf(info[7], "%f/%f%n", &floor, &ceiling, &n) == 2 && !info[7][n] &&
          floor >= -MAX_CLIENT_INFO_EXTENT && ceiling <= MAX_CLIENT_INFO_EXTENT &&
          ceiling - floor >= MIN_CLIENT_INFO_HEIGHT) {
        ci->standingFloor = floor;
        ci->standingCeiling = ceiling;
      } else {
        Cg_Warn("Invalid standing box \"%s\" for %s\n", info[7], ci->name);
      }
    }

    // ensure we were able to load everything
    if (!Cg_ValidateSkin(ci)) {

      if (!q_strcmp(s, DEFAULT_CLIENT_INFO)) {
        Cg_Error("Failed to load default client info\n");
      }
    }

    ci->legs->bounds = PM_BOUNDS;

    ci->legs->radius = Box3_Size(ci->legs->bounds).z / 2.0;

    // load sound files if we're in-game
    if (*cgi.state > CL_DISCONNECTED) {
      cgi.LoadClientModelSamples(ci->model, ci->torso->mesh->sounds);
    }
  }
}

/**
 * @brief Load all client info strings from the server.
 */
void Cg_LoadClients(void) {

  memset(cgState.clients, 0, sizeof(cgState.clients));

  for (int32_t i = 0; i < MAX_CLIENTS; i++) {
    ClientGameClientInfo *ci = &cgState.clients[i];
    const char *s = cgi.ConfigString(CS_CLIENTS + i);

    if (!*s) {
      continue;
    }

    Cg_LoadClient(ci, s);

    if (i) {
      cgi.LoadingProgress(-1, ci->model);
    }
  }

  memset(&cgState.forceSkin, 0, sizeof(cgState.forceSkin));

  if (*cg_forceSkin->string) {
    Cg_LoadClient(&cgState.forceSkin, va("-1\\newbie\\%s\\default\\default\\default\\default", cg_forceSkin->string));
  }
}

/**
 * @brief Fs_Enumerator data for `Cg_SkinAutocomplete_f`.
 */
typedef struct {
  const char *partial;
  List *matches;
} ClientGameSkinAutocomplete;

/**
 * @brief Fs_Enumerator for `Cg_SkinAutocomplete_ModelEnumerate`, appending a `model/skin`
 * match for each resolved `players/<model>/<skin>.skin` file that begins with the partial.
 */
static void Cg_SkinAutocomplete_SkinEnumerate(const char *path, void *data) {

  const ClientGameSkinAutocomplete *autocomplete = (ClientGameSkinAutocomplete *) data;

  char name[MAX_QPATH];
  StripExtension(path + strlen("players/"), name);

  if (!q_strncasecmp(name, autocomplete->partial, strlen(autocomplete->partial))) {
    cgi.AutocompleteMatch(autocomplete->matches, name, NULL);
  }
}

/**
 * @brief Fs_Enumerator for `Cg_SkinAutocomplete_f`, descending into each `players/<model>`
 * directory to resolve its `.skin` files.
 */
static void Cg_SkinAutocomplete_ModelEnumerate(const char *path, void *data) {
  cgi.EnumerateFiles(va("%s/*.skin", path), Cg_SkinAutocomplete_SkinEnumerate, data);
}

/**
 * @brief AutocompleteFunc for the `skin` cvar, matching against all resolvable
 * `model/skin` combinations beneath `players/`.
 */
void Cg_SkinAutocomplete_f(const uint32_t argi, List *matches) {

  const ClientGameSkinAutocomplete autocomplete = {
    .partial = cgi.Argv(argi),
    .matches = matches
  };

  cgi.EnumerateFiles("players/*", Cg_SkinAutocomplete_ModelEnumerate, (void *) &autocomplete);
}

/**
 * @brief Returns the next animation to advance to, defaulting to a no-op.
 */
static EntityAnimation Cg_NextAnimation(const ClientEntityAnimation *a) {

  switch (a->animation) {
    case ANIM_BOTH_DEATH1:
    case ANIM_BOTH_DEATH2:
    case ANIM_BOTH_DEATH3:
      return a->animation + 1;

    case ANIM_TORSO_DROP:
      return ANIM_TORSO_RAISE;

    case ANIM_TORSO_GESTURE:
    case ANIM_TORSO_ATTACK1:
    case ANIM_TORSO_RAISE:
      return ANIM_TORSO_STAND1;

    case ANIM_LEGS_LAND1:
    case ANIM_LEGS_LAND2:
    case ANIM_LEGS_TURN:
      return ANIM_LEGS_IDLE;

    default:
      return a->animation;
  }
}

/**
 * @brief Initiates a ragdoll animation on a client corpse. Jump a few frames back into the
 * death animation that preceeded the dead animation.
 */
void Cg_ClientRagdoll(ClientEntity *ent) {

  switch (ent->animation1.animation) {
    case ANIM_BOTH_DEAD1:
      ent->animation1.animation = ANIM_BOTH_DEATH1;
      ent->animation2.animation = ANIM_BOTH_DEATH1;
      break;
    case ANIM_BOTH_DEAD2:
      ent->animation1.animation = ANIM_BOTH_DEATH2;
      ent->animation2.animation = ANIM_BOTH_DEATH2;
      break;
    case ANIM_BOTH_DEAD3:
      ent->animation1.animation = ANIM_BOTH_DEATH3;
      ent->animation2.animation = ANIM_BOTH_DEATH3;
      break;
    default:
      return;
  }

  const ClientGameClientInfo *ci = Cg_ClientInfo(ent);
  if (!ci->torso) {
    return;
  }

  const RenderMeshAnimation *death = &ci->torso->mesh->animations[ent->animation1.animation];

  const uint32_t frameDuration = 1000 / death->hz;
  const uint32_t animDuration = death->numFrames * frameDuration;
  const uint32_t ragdollFrames = 300 / frameDuration;
  const uint32_t ragdollDuration = ragdollFrames * frameDuration;

  ent->animation1.time = cgi.client->unclampedTime - animDuration + ragdollDuration;
  ent->animation2.time = cgi.client->unclampedTime - animDuration + ragdollDuration;
}

/**
 * @brief Resolve the frames and interpolation fractions for the specified animation
 * and entity. If a non-looping animation has completed, proceed to the next
 * animation in the sequence.
 */
static void Cg_AnimateClientEntity_(const RenderModel *model, ClientEntityAnimation *a) {
  const RenderMeshModel *mesh = model->mesh;

  if ((int32_t) a->animation > mesh->numAnimations) {
    Cg_Warn("Invalid animation: %s: %d\n", model->media.name, a->animation);
    return;
  }

  const RenderMeshAnimation *anim = &mesh->animations[a->animation];

  if (!anim->numFrames || !anim->hz) {
    Cg_Warn("Bad animation sequence: %s: %d\n", model->media.name, a->animation);
    return;
  }

  const uint32_t elapsedTime = cgi.client->unclampedTime - a->time;
  const uint32_t frameDuration = 1000 / anim->hz;
  const uint32_t animDuration = anim->numFrames * frameDuration;

  int32_t frame = elapsedTime / frameDuration;

  if (elapsedTime >= animDuration) { // to loop, or not to loop

    if (!anim->loopedFrames) {
      const EntityAnimation next = Cg_NextAnimation(a);
      if (next == a->animation) { // no change, just stay put
        a->oldFrame = a->frame;
        a->lerp = a->fraction = 1.0;
        return;
      }

      a->animation = next; // or move into the next animation
      a->time = cgi.client->unclampedTime;

      Cg_AnimateClientEntity_(model, a);
      return;
    }

    frame = (frame - anim->numFrames) % anim->loopedFrames;
  }

  if (a->reverse) {
    frame = (anim->numFrames - 1) - frame;
  }

  frame = anim->firstFrame + frame;

  if (frame != a->frame) {
    if (a->frame == -1) {
      a->oldFrame = frame;
      a->frame = frame;
    } else {
      a->oldFrame = a->frame;
      a->frame = frame;
    }
  }

  a->lerp = (elapsedTime % frameDuration) / (float) frameDuration;
  a->fraction = Clampf01(elapsedTime / (float) animDuration);
}

/**
 * @brief Runs the animation sequences for the specified entity, setting the frame
 * indexes and interpolation fractions for the specified renderer entities.
 */
static void Cg_AnimateClientEntity(ClientEntity *ent, RenderEntity *torso, RenderEntity *legs) {

  Cg_AnimateClientEntity_(torso->model, &ent->animation1);

  torso->frame = ent->animation1.frame;
  torso->oldFrame = ent->animation1.oldFrame;
  torso->lerp = ent->animation1.lerp;
  torso->backLerp = 1.0 - ent->animation1.lerp;

  Cg_AnimateClientEntity_(torso->model, &ent->animation2);

  legs->frame = ent->animation2.frame;
  legs->oldFrame = ent->animation2.oldFrame;
  legs->lerp = ent->animation2.lerp;
  legs->backLerp = 1.0 - ent->animation2.lerp;
}

/**
 * @brief The min velocity we should apply leg rotation on.
 */
#define CLIENT_LEGS_SPEED_EPSILON .5f

/**
 * @brief The max yaw that we'll rotate the legs by when moving left/right.
 */
#define CLIENT_LEGS_YAW_MAX 65.f

/**
 * @brief Clamp angle
 */
#define CLIENT_LEGS_CLAMP (CLIENT_LEGS_YAW_MAX * 1.5f)

/**
 * @brief The speed that the legs will catch up to the current leg yaw.
 */
#define CLIENT_LEGS_YAW_LERP_SPEED 240.f

/**
 * @brief Rotates a current angle toward an ideal value at the given angular speed in degrees per second.
 */
static inline float Cg_CalculateAngle(const float speed, float current, float ideal) {
  current = AngleMod(current);
  ideal = AngleMod(ideal);

  if (current == ideal) {
    return current;
  }

  float move = ideal - current;

  if (ideal > current) {
    if (move >= 180.0) {
      move = move - 360.0;
    }
  } else {
    if (move <= -180.0) {
      move = move + 360.0;
    }
  }

  if (move > 0) {
    if (move > speed) {
      move = speed;
    }
  } else {
    if (move < -speed) {
      move = -speed;
    }
  }

  return AngleMod(current + move);
}

/**
 * @brief Resolves legs angles using the client entity's velocity. This allows the player torso
 * to face the direction of the player's view, and their legs to face the direction of the player's
 * movement. The leg-to-torso delta angle is clamped so that we don't get Totally Krossed Out like
 * Kris Kross. Those who were sentient in 1992 will understand.
 *
 * Models flagged `fixedlegs` in their `animation.cfg` opt out of this entirely: their legs
 * always face the same direction as the torso, with no independent yaw or turn animation.
 */
static void Cg_RotateClientLegs(const ClientGameClientInfo *ci, ClientEntity *ent, RenderEntity *legs) {

  if (ci->legs->mesh->flags & MESH_MODEL_FIXED_LEGS) {
    ent->legsYaw = ent->legsCurrentYaw = ent->angles.y;
    return;
  }

  Vec3 right;
  Vec3_Vectors(legs->angles, NULL, &right, NULL);

  Vec3 moveDir;
  moveDir = Vec3_Subtract(ent->prev.origin, ent->current.origin);
  moveDir.z = 0.f; // don't care about z, just x/y

  if (ent->animation2.animation < ANIM_LEGS_SWIM) {
    if (Vec3_Length(moveDir) > CLIENT_LEGS_SPEED_EPSILON) {
      moveDir = Vec3_Normalize(moveDir);
      float legsYaw = Vec3_Dot(moveDir, right) * CLIENT_LEGS_YAW_MAX;

      if (ent->animation2.animation == ANIM_LEGS_BACK || ent->animation2.reverse) {
        legsYaw = -legsYaw;
      }

      ent->legsYaw = ent->angles.y + legsYaw;
    } else {
      ent->legsYaw = ent->angles.y;
    }
  } else {

    const float angleDiff = SmallestAngleBetween(ent->legsYaw, ent->angles.y);

    if (ent->animation2.animation == ANIM_LEGS_TURN || fabsf(angleDiff) > CLIENT_LEGS_YAW_MAX) {
      ent->legsYaw = ent->angles.y;

      // change animation as well
      if (ent->animation2.animation == ANIM_LEGS_IDLE) {
        ent->animation2.time = cgi.client->unclampedTime;
        ent->animation2.frame = ent->animation2.oldFrame = -1;
        ent->animation2.lerp = ent->animation2.fraction = 0;
      }
    }
  }

  ent->legsCurrentYaw = Cg_CalculateAngle(CLIENT_LEGS_YAW_LERP_SPEED * MILLIS_TO_SECONDS(cgi.client->frameMsec), ent->legsCurrentYaw, ent->legsYaw);

  const float angleDelta = AngleMod(ent->legsCurrentYaw - ent->legsYaw + 180.0f) - 180.0f;

  ent->legsCurrentYaw = AngleMod(ent->legsYaw + clamp(angleDelta, -CLIENT_LEGS_CLAMP, CLIENT_LEGS_CLAMP));

  if (fabsf(SmallestAngleBetween(ent->legsYaw, ent->legsCurrentYaw)) > 1) {
    if (ent->animation2.animation == ANIM_LEGS_IDLE) {
      ent->animation2.time = cgi.client->unclampedTime;
      ent->animation2.animation = ANIM_LEGS_TURN;
    }
  } else {
    if (ent->animation2.animation == ANIM_LEGS_TURN) {
      ent->animation2.time = cgi.client->unclampedTime;
      ent->animation2.animation = ANIM_LEGS_IDLE;
    }
  }
}

/**
 * @brief The tail of the `Cg_ClientInfo` chain: the slot the entity names.
 */
static ClientGameClientInfo *Cg_ClientInfo_Common(const ClientEntity *ent) {

  // a corpse names a slot of its own, holding the client info it died wearing, so that it is
  // not repainted by its owner changing skin and does not fall back to the default model when
  // they disconnect and their entry is cleared. The mask is what the slot was assigned with.
  if (ent->current.effects & EF_CORPSE) {
    return &cgState.corpses[ent->current.client & (MAX_CORPSES - 1)];
  }

  return &cgState.clients[ent->current.client];
}

ClientInfo Cg_ClientInfo = Cg_ClientInfo_Common;

/**
 * @brief Adds the numerous render entities which comprise a given client (player)
 * entity: head, torso, legs, weapon, flags, etc.
 */
void Cg_AddClientEntity(ClientEntity *ent, RenderEntity *e) {

  const EntityState *s = &ent->current;
  ClientGameClientInfo *ci = Cg_ClientInfo(ent);

  if (!ci->head || !ci->torso || !ci->legs) {
    const int32_t cs = (s->effects & EF_CORPSE) ? CS_CORPSES : CS_CLIENTS;
    if (*cgi.ConfigString(cs + s->client)) {
      Cg_Warn("Invalid client info: %d\n", s->client);
    }
    return;
  }

  // deal with our own player model
  if (ent == cgi.client->entity) {
    if (!cgi.client->thirdPerson) {
      e->effects |= EF_SELF | EF_NO_DRAW;

      // keep our shadow underneath us using the predicted origin
      e->origin.x = cgi.view->origin.x;
      e->origin.y = cgi.view->origin.y;
    } else {
      // in third-person, treat ourselves exactly like any other client
      e->origin.z -= ent->stepOffset;
      Cg_BreathTrail(ent);
    }
  } else {
    e->origin.z -= ent->stepOffset;
    Cg_BreathTrail(ent);
  }

  // set tints
  if (ci->shirt.a) {
    e->tints[0] = ci->shirt.vec4;
  }

  if (ci->pants.a) {
    e->tints[1] = ci->pants.vec4;
  }

  if (ci->helmet.a) {
    e->tints[2] = ci->helmet.vec4;
  }

  RenderEntity head, torso, legs;

  // copy the specified entity to all body segments
  head = torso = legs = *e;

  if ((ent->current.effects & EF_CORPSE) == 0) {
    Cg_RotateClientLegs(ci, ent, &legs);
  }

  ClientGameClientInfo *skin = ci;

  // force the preferred skin on all _other_ players, not on ourselves
  if (cgState.forceSkin.torso && ent != cgi.client->entity) {
    skin = &cgState.forceSkin;
  }

  legs.model = skin->legs;
  legs.angles.y = ent->legsCurrentYaw;
  legs.angles.x = legs.angles.z = 0.0; // legs only use yaw
  legs.bounds = legs.model->bounds;
  memcpy(legs.skins, skin->legsSkins, sizeof(legs.skins));
  legs.hasSkins = true;

  // the model is built for PM_BOUNDS; a client whose standing box is another
  // size has the whole rig scaled to its height and seated on its floor
  legs.scale = e->scale * (ci->standingCeiling - ci->standingFloor) / Box3_Size(PM_BOUNDS).z;
  legs.origin.z += ci->standingFloor - legs.scale * PM_BOUNDS.mins.z;

  torso.model = skin->torso;
  torso.origin = Vec3_Zero();
  torso.angles.y = ent->angles.y - legs.angles.y; // legs twisted already, we just need to pitch/roll
  if (torso.model->mesh->flags & MESH_MODEL_FIXED_TORSO) {
    torso.angles.x = 0.0; // fixedtorso: never pitch independently of the legs
  }
  torso.bounds = torso.model->bounds;
  memcpy(torso.skins, skin->torsoSkins, sizeof(torso.skins));
  torso.hasSkins = true;

  head.model = skin->head;
  head.origin = Vec3_Zero();
  head.angles.y = 0.0;
  head.bounds = head.model->bounds;
  memcpy(head.skins, skin->headSkins, sizeof(head.skins));
  head.hasSkins = true;

  Cg_AnimateClientEntity(ent, &torso, &legs);

  RenderEntity *rLegs = cgi.AddEntity(cgi.view, &legs);

  if (!rLegs) {
    return; // if the legs were culled, we're done
  }

  torso.parent = rLegs;
  torso.tag = "tag_torso";

  RenderEntity *rTorso = cgi.AddEntity(cgi.view, &torso);
  assert(rTorso);

  head.parent = rTorso;
  head.tag = "tag_head";

  RenderEntity *rHead = cgi.AddEntity(cgi.view, &head);
  assert(rHead);

  RenderEntity *rWeapon = NULL;
  if (s->model2) {
    rWeapon = cgi.AddEntity(cgi.view, &(const RenderEntity) {
      .parent = rTorso,
      .tag = "tag_weapon",
      .scale = e->scale,
      .model = cgi.client->models[s->model2],
      .effects = e->effects,
      .color = e->color,
      .shell = e->shell,
    });

    assert(rWeapon);

    // cache the muzzle position post-animation for muzzle flash and beam alignment

    const Vec3 cfgMuzzle = rWeapon->model->mesh->config.link.muzzle;
    if (!Vec3_Equal(cfgMuzzle, Vec3_Zero())) {
      ci->weaponMuzzle = Mat4_Transform(rWeapon->matrix, cfgMuzzle);
    } else {
      ci->weaponMuzzle = rWeapon->origin;
    }
  }

  RenderEntity *rFlag = NULL;
  if (s->model3) {
    rFlag = cgi.AddEntity(cgi.view, &(const RenderEntity) {
      .parent = rTorso,
      .tag = "tag_head",
      .scale = e->scale,
      .model = cgi.client->models[s->model3],
      .effects = e->effects,
      .color = e->color,
      .shell = e->shell,
    });

    assert(rFlag);
  }

  if (s->model4) {
    Cg_Warn("Unsupported model_index4\n");
  }
}
