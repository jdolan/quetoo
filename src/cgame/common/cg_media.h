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

#pragma once

#include "cg_types.h"

#if defined(__CG_LOCAL_H__)
extern SoundSample *cg_sampleBlasterFire;
extern SoundSample *cg_sampleBlasterHit;
extern SoundSample *cg_sampleShotgunFire;
extern SoundSample *cg_sampleSupershotgunFire;
extern SoundSample *cg_sampleMachinegunFire[3];
extern SoundSample *cg_sampleMachinegunHit[3];
extern SoundSample *cg_sampleGrenadelauncherFire;
extern SoundSample *cg_sampleRocketlauncherFire;
extern SoundSample *cg_sampleHyperblasterFire;
extern SoundSample *cg_sampleHyperblasterHit;
extern SoundSample *cg_sampleLightningFire;
extern SoundSample *cg_sampleLaserFire;
extern SoundSample *cg_sampleLightningDischarge;
extern SoundSample *cg_sampleRailgunFire;
extern SoundSample *cg_sampleBfgFire;
extern SoundSample *cg_sampleBfgHit;

#if defined(G_HOOK)
extern SoundSample *cg_sampleHookHit;
#endif

extern SoundSample *cg_sampleQuakeShotgunFire;
extern SoundSample *cg_sampleQuakeSupershotgunFire;
extern SoundSample *cg_sampleQuakeNailgunFire;
extern SoundSample *cg_sampleQuakeSupernailgunFire;
extern SoundSample *cg_sampleQuakeNailHit;
extern SoundSample *cg_sampleQuakeGrenadelauncherFire;
extern SoundSample *cg_sampleQuakeRocketlauncherFire;

extern SoundSample *cg_sampleExplosion;
extern SoundSample *cg_sampleTeleport;
extern SoundSample *cg_sampleRespawn;
extern SoundSample *cg_sampleSparks;
extern SoundSample *cg_sampleFire;
extern SoundSample *cg_sampleSteam;

extern SoundSample *cg_sampleRain;
extern SoundSample *cg_sampleSnow;
extern SoundSample *cg_sampleAsh;
extern SoundSample *cg_sampleUnderwater;
extern SoundSample *cg_sampleHits[2];
extern SoundSample *cg_sampleGib;

extern RenderAtlasImage *cg_spriteParticle;
extern RenderAtlasImage *cg_spriteParticle2;
extern RenderAtlasImage *cg_spriteParticle3;
extern RenderAtlasImage *cg_spriteFlash;
extern RenderAtlasImage *cg_spriteRing;
extern RenderAtlasImage *cg_spriteBlasterFlash;
extern RenderAtlasImage *cg_spriteAnisoFlare01;
extern RenderAtlasImage *cg_spriteRain;
extern RenderAtlasImage *cg_spriteSnow;
extern RenderAtlasImage *cg_spriteAsh;
extern RenderAtlasImage *cg_spriteSmoke;
extern RenderAtlasImage *cg_spriteFlame;
extern RenderAtlasImage *cg_spriteSpark;
extern RenderAtlasImage *cg_spriteBubble;
extern RenderAtlasImage *cg_spriteTeleport;
extern RenderAtlasImage *cg_spriteTeleportCore;
extern RenderAtlasImage *cg_spriteSteam;
extern RenderAtlasImage *cg_spriteInactive;
extern RenderAtlasImage *cg_spritePlasmaVar01;
extern RenderAtlasImage *cg_spritePlasmaVar02;
extern RenderAtlasImage *cg_spritePlasmaVar03;
extern RenderAtlasImage *cg_spriteBlob01;
extern RenderAtlasImage *cg_spriteElectro02;
extern RenderAtlasImage *cg_spriteExplosionFlash;
extern RenderAtlasImage *cg_spriteExplosionGlow;
extern RenderAtlasImage *cg_spriteSplash0203;
extern RenderAtlasImage *cg_spriteImpactSpark01Dot;
extern RenderAtlasImage *cg_spritePuffCloud;
extern RenderAtlasImage *cg_spriteWaterCircle;
extern RenderAtlasImage *cg_spriteWaterRing;
extern RenderAtlasImage *cg_spriteWaterRing2;
extern RenderAtlasImage *cg_spriteAbstract01;
extern RenderAtlasImage *cg_spriteNodeWait;
extern RenderAtlasImage *cg_spriteNodeSlow;

extern RenderImage *cg_beamHook;
extern RenderImage *cg_beamArrow;
extern RenderImage *cg_beamLine;
extern RenderImage *cg_beamRail;
extern RenderImage *cg_beamLightning;
extern RenderImage *cg_beamTracer;
extern RenderImage *cg_beamTail;

extern RenderAnimation *cg_spriteExplosion;
extern RenderAnimation *cg_spriteExplosionRing02;
extern RenderAnimation *cg_spriteRocketFlame;
extern RenderAnimation *cg_spriteBlasterFlame;
extern RenderAnimation *cg_spriteSmoke04;
extern RenderAnimation *cg_spriteSmoke05;
extern RenderAnimation *cg_spriteBlasterRing;
extern RenderAnimation *cg_spriteBfgExplosion1;
extern RenderAnimation *cg_spriteBfgExplosion2;
extern RenderAnimation *cg_spriteBfgExplosion3;
extern RenderAnimation *cg_spritePoof01;
extern RenderAnimation *cg_spritePoof02;
extern RenderAnimation *cg_spriteBlood01;
extern RenderAnimation *cg_spriteElectro01;
extern RenderAnimation *cg_spriteFireball01;
extern RenderAnimation *cg_spriteImpactSpark01;
extern RenderAnimation *cg_spriteHyperball01;
extern RenderAnimation *cg_spriteFizz01;

extern RenderAtlasImage *cg_decalBullet[3];
extern RenderAtlasImage *cg_decalBlood[4];
extern RenderAtlasImage *cg_decalBurn[4];
extern RenderAtlasImage *cg_decalSlug[4];

extern Framebuffer *cg_framebuffer;

void Cg_CreateFramebuffer(void);
void Cg_DestroyFramebuffer(void);
void Cg_LoadMedia(void);
void Cg_FreeMedia(void);
#endif
