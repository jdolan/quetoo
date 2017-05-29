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

#include "s_local.h"

/* ALC_EXT_EFX */
LPALGENEFFECTS alGenEffects;
LPALDELETEEFFECTS alDeleteEffects;
LPALISEFFECT alIsEffect;
LPALEFFECTI alEffecti;
LPALEFFECTIV alEffectiv;
LPALEFFECTF alEffectf;
LPALEFFECTFV alEffectfv;
LPALGETEFFECTI alGetEffecti;
LPALGETEFFECTIV alGetEffectiv;
LPALGETEFFECTF alGetEffectf;
LPALGETEFFECTFV alGetEffectfv;

LPALGENFILTERS alGenFilters;
LPALDELETEFILTERS alDeleteFilters;
LPALISFILTER alIsFilter;
LPALFILTERI alFilteri;
LPALFILTERIV alFilteriv;
LPALFILTERF alFilterf;
LPALFILTERFV alFilterfv;
LPALGETFILTERI alGetFilteri;
LPALGETFILTERIV alGetFilteriv;
LPALGETFILTERF alGetFilterf;
LPALGETFILTERFV alGetFilterfv;

LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

int ALAD_ALC_EXT_EFX;

static ALCdevice *device;

static
void* get_proc(const char *namez) {
	void* proc = (void*)alGetProcAddress(namez);

	if (!proc)
		proc = (void*)alcGetProcAddress(device, namez);

    return proc;
}

static int has_ext(const char *ext) {
	return alIsExtensionPresent(ext) || alcIsExtensionPresent(device, ext);
}

static void load_ALC_EXT_EFX(void) {
	if(!ALAD_ALC_EXT_EFX) return;
	alGenEffects = (LPALGENEFFECTS)get_proc("alGenEffects");
	alDeleteEffects = (LPALDELETEEFFECTS)get_proc("alDeleteEffects");
	alIsEffect = (LPALISEFFECT)get_proc("alIsEffect");
	alEffecti = (LPALEFFECTI)get_proc("alEffecti");
	alEffectiv = (LPALEFFECTIV)get_proc("alEffectiv");
	alEffectf = (LPALEFFECTF)get_proc("alEffectf");
	alEffectfv = (LPALEFFECTFV)get_proc("alEffectfv");
	alGetEffecti = (LPALGETEFFECTI)get_proc("alGetEffecti");
	alGetEffectiv = (LPALGETEFFECTIV)get_proc("alGetEffectiv");
	alGetEffectf = (LPALGETEFFECTF)get_proc("alGetEffectf");
	alGetEffectfv = (LPALGETEFFECTFV)get_proc("alGetEffectfv");

	alGenFilters = (LPALGENFILTERS)get_proc("alGenFilters");
	alDeleteFilters = (LPALDELETEFILTERS)get_proc("alDeleteFilters");
	alIsFilter = (LPALISFILTER)get_proc("alIsFilter");
	alFilteri = (LPALFILTERI)get_proc("alFilteri");
	alFilteriv = (LPALFILTERIV)get_proc("alFilteriv");
	alFilterf = (LPALFILTERF)get_proc("alFilterf");
	alFilterfv = (LPALFILTERFV)get_proc("alFilterfv");
	alGetFilteri = (LPALGETFILTERI)get_proc("alGetFilteri");
	alGetFilteriv = (LPALGETFILTERIV)get_proc("alGetFilteriv");
	alGetFilterf = (LPALGETFILTERF)get_proc("alGetFilterf");
	alGetFilterfv = (LPALGETFILTERFV)get_proc("alGetFilterfv");

	alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)get_proc("alGenAuxiliaryEffectSlots");
	alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)get_proc("alDeleteAuxiliaryEffectSlots");
	alIsAuxiliaryEffectSlot = (LPALISAUXILIARYEFFECTSLOT)get_proc("alIsAuxiliaryEffectSlot");
	alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)get_proc("alAuxiliaryEffectSloti");
	alAuxiliaryEffectSlotiv = (LPALAUXILIARYEFFECTSLOTIV)get_proc("alAuxiliaryEffectSlotiv");
	alAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)get_proc("alAuxiliaryEffectSlotf");
	alAuxiliaryEffectSlotfv = (LPALAUXILIARYEFFECTSLOTFV)get_proc("alAuxiliaryEffectSlotfv");
	alGetAuxiliaryEffectSloti = (LPALGETAUXILIARYEFFECTSLOTI)get_proc("alGetAuxiliaryEffectSloti");
	alGetAuxiliaryEffectSlotiv = (LPALGETAUXILIARYEFFECTSLOTIV)get_proc("alGetAuxiliaryEffectSlotiv");
	alGetAuxiliaryEffectSlotf = (LPALGETAUXILIARYEFFECTSLOTF)get_proc("alGetAuxiliaryEffectSlotf");
	alGetAuxiliaryEffectSlotfv = (LPALGETAUXILIARYEFFECTSLOTFV)get_proc("alGetAuxiliaryEffectSlotfv");
}

static int find_extensionsAL(void) {
	ALAD_ALC_EXT_EFX = has_ext("ALC_EXT_EFX");
	return 1;
}

int aladLoadAL(void) {
	ALCcontext *context = alcGetCurrentContext();

	if (!context) return 0;

	device = alcGetContextsDevice(context);

	if (!device) return 0;

	if (!find_extensionsAL()) return 0;
	load_ALC_EXT_EFX();
	return 1;
}