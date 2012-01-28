/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "s_local.h"
#include "client.h"

cvar_t *s_music_volume;

static s_music_t default_music;

static const char *MUSIC_TYPES[] = { ".ogg", NULL };

/*
 * S_LoadMusic
 */
static s_music_t *S_LoadMusic(const char *name) {
	void *buf;
	int i, len;
	SDL_RWops *rw;
	static s_music_t music;

	memset(&music, 0, sizeof(music));

	buf = NULL;
	rw = NULL;

	snprintf(music.name, sizeof(music.name), "music/%s", name);

	i = 0;
	while (MUSIC_TYPES[i]) {

		StripExtension(music.name, music.name);
		strcat(music.name, MUSIC_TYPES[i++]);

		if ((len = Fs_LoadFile(music.name, &buf)) == -1)
			continue;

		if (!(rw = SDL_RWFromMem(buf, len))) {
			Fs_FreeFile(buf);
			continue;
		}

		if (!(music.music = Mix_LoadMUS_RW(rw))) {
			Com_Warn("S_LoadMusic: %s.\n", Mix_GetError());

			SDL_FreeRW(rw);

			Fs_FreeFile(buf);
			continue;
		}

		music.buffer = buf;

		return &music;
	}

	// warn of failures to load non-default tracks
	if (strncmp(name, "track", 5)) {
		Com_Warn("S_LoadMusic: Failed to load %s.\n", name);
	}

	return NULL;
}

/*
 * S_FreeMusic
 */
static void S_FreeMusic(s_music_t *music) {

	if (!music)
		return;

	if (music->music)
		Mix_FreeMusic(music->music);

	if (music->buffer)
		Fs_FreeFile(music->buffer);
}

/*
 * S_FreeMusics
 */
static void S_FreeMusics(void) {
	int i;

	for (i = 0; i < MAX_MUSICS; i++)
		S_FreeMusic(&s_env.musics[i]);

	memset(s_env.musics, 0, sizeof(s_env.musics));
	s_env.num_musics = 0;

	s_env.active_music = NULL;
}

/*
 * S_LoadMusics
 */
void S_LoadMusics(void) {
	char shuffle[MAX_MUSICS * MAX_QPATH];
	s_music_t *music;
	int i;

	S_FreeMusics();

	// if no music is provided, use the default tracks
	if (!cl.config_strings[CS_MUSICS + 1][0]) {

		memset(shuffle, 0, sizeof(shuffle));
		i = 1;

		// shuffle the order of the default tracks to keep it fresh
		while (i < MAX_MUSICS + 1) {

			char *s = cl.config_strings[CS_MUSICS + i];
			const int j = rand() % MAX_MUSICS;

			sprintf(s, "track%d", j);

			if (strstr(shuffle, va("%s|", s)))
				continue;

			strcat(shuffle, s);
			strcat(shuffle, "|");

			i++;
		}
	}

	for (i = 1; i < MAX_MUSICS + 1; i++) {

		if (!cl.config_strings[CS_MUSICS + i][0])
			break;

		if (!(music = S_LoadMusic(cl.config_strings[CS_MUSICS + i])))
			continue;

		memcpy(&s_env.musics[s_env.num_musics++], music, sizeof(s_music_t));

		Com_Debug("Loaded music %s\n", music->name);
	}

	Com_Print("Loaded %d tracks\n", s_env.num_musics);
}

/*
 * S_StopMusic
 */
static void S_StopMusic(void) {

	Mix_HaltMusic();

	s_env.active_music = NULL;
}

/*
 * S_PlayMusic
 */
static void S_PlayMusic(s_music_t *music) {

	Mix_PlayMusic(music->music, 1);

	s_env.active_music = music;
}

/*
 * S_NextMusic
 */
static s_music_t *S_NextMusic(void) {
	s_music_t *music;

	music = &default_music;

	if (s_env.num_musics) { // select a new track
		int i;

		for (i = 0; i < MAX_MUSICS; i++) {
			if (s_env.active_music == &s_env.musics[i])
				break;
		}

		if (i < MAX_MUSICS)
			i++;
		else
			i = 0;

		music = &s_env.musics[i % s_env.num_musics];
	}

	return music;
}

/*
 * S_FrameMusic
 */
void S_FrameMusic(void) {
	s_music_t *music;

	if (s_music_volume->modified) {

		if (s_music_volume->value > 1.0)
			s_music_volume->value = 1.0;

		if (s_music_volume->value < 0.0)
			s_music_volume->value = 0.0;

		if (s_music_volume->value)
			Mix_VolumeMusic(s_music_volume->value * 255);
		else
			S_StopMusic();
	}

	if (!s_music_volume->value)
		return;

	music = &default_music;

	if (cls.state == CL_ACTIVE) { // try level-specific music

		if (!Mix_PlayingMusic() || (s_env.active_music == &default_music)) {

			if ((music = S_NextMusic()) != s_env.active_music)
				S_StopMusic();
		}
	} else { // select the default music

		if (s_env.active_music != &default_music)
			S_StopMusic();
	}

	if (!Mix_PlayingMusic()) // play it
		S_PlayMusic(music);
}

/*
 * S_NextTrack_f
 */
static void S_NextTrack_f(void) {

	S_PlayMusic(S_NextMusic());
}

/*
 * S_InitMusic
 */
void S_InitMusic(void) {
	s_music_t *music;

	s_music_volume = Cvar_Get("s_music_volume", "0.25", CVAR_ARCHIVE,
			"Music volume level.");

	Cmd_AddCommand("s_next_track", S_NextTrack_f, "Play the next music track.");

	S_FreeMusics();

	memset(&default_music, 0, sizeof(default_music));

	music = S_LoadMusic("default");

	if (music)
		memcpy(&default_music, music, sizeof(default_music));
}

/*
 * S_ShutdownMusic
 */
void S_ShutdownMusic(void) {

	S_StopMusic();

	S_FreeMusics();

	S_FreeMusic(&default_music);
}
