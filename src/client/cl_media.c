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

#include <Objectively/HashTable.h>

#include "cl_local.h"

/**
 * @brief HashTable enumerateEntries callback for manifest entry validation/download.
 */
static void Cl_CheckManifestEntry_(const HashTable *table, ident key, ident val, ident data) {
  (void) table;
  const cm_manifest_entry_t *entry = (const cm_manifest_entry_t *) val;
  if (Fs_Exists(entry->path)) {
    if (!Cm_CheckManifestEntry(entry)) {
      Com_Warn("%s differs from server (expected %s)\n", entry->path, entry->hash);
    }
  } else {
    Cl_CheckOrDownloadFile(entry->path);
  }
}

/**
 * @brief Entry point for file downloads, or "precache" from server. Attempt to
 * download the .pk3 archive, then the .mf manifest and all assets it references.
 * Once all precache checks are completed, we load media and ask the server to
 * begin sending us frames.
 */
void Cl_RequestNextDownload(void) {

  if (cls.state < CL_CONNECTED) {
    return;
  }

  // check pk3 archive (may contain everything including manifest and bsp)
  if (cl.precache_check == CS_PK3) {
    cl.precache_check = CS_MANIFEST;

    if (*cl.config_strings[CS_PK3] != '\0') {
      Cl_CheckOrDownloadFile(cl.config_strings[CS_PK3]);
    }
  }

  // download the manifest and all assets it references (including the bsp)
  if (cl.precache_check == CS_MANIFEST) {
    cl.precache_check++;

    if (*cl.config_strings[CS_MANIFEST] != '\0') {
      Cl_CheckOrDownloadFile(cl.config_strings[CS_MANIFEST]);

      HashTable *manifest = Cm_ReadManifest(cl.config_strings[CS_MANIFEST]);
      if (!manifest) {
        Com_Error(ERROR_DROP, "Failed to read %s\n", cl.config_strings[CS_MANIFEST]);
      }

      $(manifest, enumerateEntries, Cl_CheckManifestEntry_, NULL);

      Cm_FreeManifest(manifest);
    }
  }

  // we're good to go, lock and load (literally)

  Cvar_ResetDeveloper();

  Cl_LoadMedia();

  Net_WriteByte(&cls.net_chan.message, CL_CMD_STRING);
  Net_WriteString(&cls.net_chan.message, va("begin %i\n", cls.spawn_count));
}


/**
 * @brief `Fs_Enumerate` function for `Cl_Mapshots`.
 */
static void Cl_Mapshots_enumerate(const char *path, void *data) {
  List *list = (List *) data;

  const size_t len = strlen(path);
  if ((len >= 4 && !strcmp(path + len - 4, ".jpg")) ||
      (len >= 4 && !strcmp(path + len - 4, ".png"))) {
    $(list, append, SDL_strdup(path));
  }
}

/**
 * @return A List of known mapshots for the given map.
 */
List *Cl_Mapshots(const char *mapname) {

  char map[MAX_QPATH];
  StripExtension(mapname, map);

  List *list = $(alloc(List), init);
  list->destroy = (ListDestroyFunc) SDL_free;
  Fs_Enumerate(va("mapshots/%s/*", Basename(map)), Cl_Mapshots_enumerate, (void *) list);

  return list;
}

/**
 * @brief Update the loading progress, handle events and update the screen.
 * This should be called periodically while loading media.
 * @param percent The percent. Positive values for absolute, negative for relative increment.
 */
void Cl_LoadingProgress(int32_t percent, const char *status) {

  if (percent < 0) {
    cls.loading.percent -= percent;
    cls.loading.percent = Mini(Maxi(0, cls.loading.percent), 99);
  } else {
    cls.loading.percent = percent;
  }

  cls.loading.status = status;

  Cl_HandleEvents();

  Cl_SendCommands();

  cls.cgame->UpdateLoading(cls.loading);

  R_BeginFrame();

  Cl_UpdateScreen();

  R_EndFrame();

  quetoo.ticks = (uint32_t) SDL_GetTicks();
}

/**
 * @brief Loads the BSP and inline models listed in the current config strings.
 */
static void Cl_LoadModels(void) {

  Cl_LoadingProgress(0, cl.config_strings[CS_BSP]);

  R_LoadModel(cl.config_strings[CS_BSP]);

  for (int32_t i = 0; i < MAX_MODELS; i++) {

    const char *str = cl.config_strings[CS_MODELS + i];
    if (*str == 0) {
      break;
    }

    Cl_LoadingProgress(-1, str);

    cl.models[i] = R_LoadModel(str);
  }
}

/**
 * @brief `Fs_Enumerator` to load all emoji into the images atlas.
 */
static void Cl_LoadImages_Emoji(const char *path, void *data) {
  R_LoadAtlasImage((r_atlas_t *) data, path, IMG_PIC);
}

/**
 * @brief Loads all images, the sky, and compiles the emoji image atlas.
 */
static void Cl_LoadImages(void) {

  Cl_LoadingProgress(-1, "compiling image atlas");

  R_LoadSky();

  r_atlas_t *atlas = R_LoadAtlas("images");
  Fs_Enumerate("pics/emoji/*", Cl_LoadImages_Emoji, atlas);

  for (int32_t i = 0; i < MAX_IMAGES; i++) {

    const char *str = cl.config_strings[CS_IMAGES + i];
    if (*str == 0) {
      break;
    }

    cl.images[i] = (r_image_t *) R_LoadAtlasImage(atlas, str, IMG_PIC);
  }

  Cl_LoadingProgress(-1, "compiling image atlas");

  R_CompileAtlas(atlas);
}

/**
 * @brief Loads all sound samples listed in the current config strings.
 */
static void Cl_LoadSounds(void) {

  Cl_LoadingProgress(-1, "sounds");

  if (*cl_chat_sound->string) {
    S_LoadSample(cl_chat_sound->string);
  }

  if (*cl_team_chat_sound->string) {
    S_LoadSample(cl_team_chat_sound->string);
  }

  for (int32_t i = 0; i < MAX_SOUNDS; i++) {

    const char *str = cl.config_strings[CS_SOUNDS + i];
    if (*str == 0) {
      break;
    }

    cl.sounds[i] = S_LoadSample(str);
  }

  for (int32_t i = 0; i < Cm_Bsp()->num_materials; i++) {
    const cm_footsteps_t *footsteps = &Cm_Bsp()->materials[i]->footsteps;

    const cm_asset_t *sample = footsteps->samples;
    for (int32_t j = 0; j < footsteps->num_samples; j++, sample++) {
      S_LoadSample(sample->name);
    }
  }
}

/**
 * @brief Loads all music tracks listed in the current config strings.
 */
static void Cl_LoadMusics(void) {

  Cl_LoadingProgress(-1, "music");

  s_music_t *const current = S_CurrentMusic();

  S_ClearPlaylist();

  for (int32_t i = 0; i < MAX_MUSICS; i++) {

    const char *str = cl.config_strings[CS_MUSICS + i];
    if (*str == 0) {
      break;
    }

    cl.musics[i] = S_LoadMusic(str);
  }

  if (!S_PlaylistContains(current)) {
    S_NextTrack_f();
  }
}

/**
 * @brief Load all game media through the relevant subsystems.
 */
void Cl_LoadMedia(void) {

  cls.cgame->FreeMedia();

  cls.state = CL_LOADING;

  List *mapshots = Cl_Mapshots(cl.config_strings[CS_BSP]);
  const size_t len = mapshots->count;

  if (len > 0) {
    const ListNode *node = mapshots->head;
    const size_t pick = (size_t) rand() % len;
    for (size_t i = 0; i < pick; i++) {
      node = node->next;
    }
    strcpy(cls.loading.mapshot, (const char *) node->data);
  } else {
    cls.loading.mapshot[0] = '\0';
  }

  $(mapshots, removeAll);
  release(mapshots);

  Cl_UpdatePrediction();

  R_BeginLoading();

  Cl_LoadModels();

  Cl_LoadImages();

  S_Stop();

  S_BeginLoading();

  Cl_LoadSounds();

  Cl_LoadMusics();

  cls.cgame->LoadMedia();

  Cl_LoadingProgress(100, "ready");

  R_EndLoading();

  S_EndLoading();

  Ui_ViewWillAppear();
}
