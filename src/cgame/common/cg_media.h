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

#pragma once

#include "cg_types.h"

#if defined(__CG_LOCAL_H__)
extern SoundSample *cgSampleBlasterFire;
extern SoundSample *cgSampleBlasterHit;
extern SoundSample *cgSampleShotgunFire;
extern SoundSample *cgSampleSupershotgunFire;
extern SoundSample *cgSampleMachinegunFire[3];
extern SoundSample *cgSampleMachinegunHit[3];
extern SoundSample *cgSampleGrenadelauncherFire;
extern SoundSample *cgSampleRocketlauncherFire;
extern SoundSample *cgSampleHyperblasterFire;
extern SoundSample *cgSampleHyperblasterHit;
extern SoundSample *cgSampleLightningFire;
extern SoundSample *cgSampleLaserFire;
extern SoundSample *cgSampleLightningDischarge;
extern SoundSample *cgSampleRailgunFire;
extern SoundSample *cgSampleBfgFire;
extern SoundSample *cgSampleBfgHit;

#if defined(G_HOOK)
extern SoundSample *cgSampleHookHit;
#endif

extern SoundSample *cgSampleQuakeShotgunFire;
extern SoundSample *cgSampleQuakeSupershotgunFire;
extern SoundSample *cgSampleQuakeNailgunFire;
extern SoundSample *cgSampleQuakeSupernailgunFire;
extern SoundSample *cgSampleQuakeNailHit;
extern SoundSample *cgSampleQuakeGrenadelauncherFire;
extern SoundSample *cgSampleQuakeRocketlauncherFire;

extern SoundSample *cgSampleExplosion;
extern SoundSample *cgSampleTeleport;
extern SoundSample *cgSampleRespawn;
extern SoundSample *cgSampleSparks;
extern SoundSample *cgSampleFire;
extern SoundSample *cgSampleSteam;

extern SoundSample *cgSampleRain;
extern SoundSample *cgSampleSnow;
extern SoundSample *cgSampleAsh;
extern SoundSample *cgSampleUnderwater;
extern SoundSample *cgSampleHits[2];
extern SoundSample *cgSampleGib;

extern RenderAtlasImage *cgSpriteParticle;
extern RenderAtlasImage *cgSpriteParticle2;
extern RenderAtlasImage *cgSpriteParticle3;
extern RenderAtlasImage *cgSpriteFlash;
extern RenderAtlasImage *cgSpriteRing;
extern RenderAtlasImage *cgSpriteBlasterFlash;
extern RenderAtlasImage *cgSpriteAnisoFlare01;
extern RenderAtlasImage *cgSpriteRain;
extern RenderAtlasImage *cgSpriteSnow;
extern RenderAtlasImage *cgSpriteAsh;
extern RenderAtlasImage *cgSpriteSmoke;
extern RenderAtlasImage *cgSpriteFlame;
extern RenderAtlasImage *cgSpriteSpark;
extern RenderAtlasImage *cgSpriteBubble;
extern RenderAtlasImage *cgSpriteTeleport;
extern RenderAtlasImage *cgSpriteTeleportCore;
extern RenderAtlasImage *cgSpriteSteam;
extern RenderAtlasImage *cgSpriteInactive;
extern RenderAtlasImage *cgSpritePlasmaVar01;
extern RenderAtlasImage *cgSpritePlasmaVar02;
extern RenderAtlasImage *cgSpritePlasmaVar03;
extern RenderAtlasImage *cgSpriteBlob01;
extern RenderAtlasImage *cgSpriteElectro02;
extern RenderAtlasImage *cgSpriteExplosionFlash;
extern RenderAtlasImage *cgSpriteExplosionGlow;
extern RenderAtlasImage *cgSpriteSplash0203;
extern RenderAtlasImage *cgSpriteImpactSpark01Dot;
extern RenderAtlasImage *cgSpritePuffCloud;
extern RenderAtlasImage *cgSpriteWaterCircle;
extern RenderAtlasImage *cgSpriteWaterRing;
extern RenderAtlasImage *cgSpriteWaterRing2;
extern RenderAtlasImage *cgSpriteAbstract01;
extern RenderAtlasImage *cgSpriteNodeWait;
extern RenderAtlasImage *cgSpriteNodeSlow;

extern RenderImage *cgBeamHook;
extern RenderImage *cgBeamArrow;
extern RenderImage *cgBeamLine;
extern RenderImage *cgBeamRail;
extern RenderImage *cgBeamLightning;
extern RenderImage *cgBeamTracer;
extern RenderImage *cgBeamTail;

extern RenderAnimation *cgSpriteExplosion;
extern RenderAnimation *cgSpriteExplosionRing02;
extern RenderAnimation *cgSpriteRocketFlame;
extern RenderAnimation *cgSpriteBlasterFlame;
extern RenderAnimation *cgSpriteSmoke04;
extern RenderAnimation *cgSpriteSmoke05;
extern RenderAnimation *cgSpriteBlasterRing;
extern RenderAnimation *cgBfgExplosion1;
extern RenderAnimation *cgSpriteBfgExplosion2;
extern RenderAnimation *cgSpriteBfgExplosion3;
extern RenderAnimation *cgSpritePoof01;
extern RenderAnimation *cgSpritePoof02;
extern RenderAnimation *cgSpriteBlood01;
extern RenderAnimation *cgSpriteElectro01;
extern RenderAnimation *cgSpriteFireball01;
extern RenderAnimation *cgSpriteImpactSpark01;
extern RenderAnimation *cgSpriteHyperball01;
extern RenderAnimation *cgSpriteFizz01;

extern RenderAtlasImage *cgDecalBullet[3];
extern RenderAtlasImage *cgDecalBlood[4];
extern RenderAtlasImage *cgDecalBurn[4];
extern RenderAtlasImage *cgDecalSlug[4];

extern Framebuffer *cgFramebuffer;

void Cg_CreateFramebuffer(void);
void Cg_DestroyFramebuffer(void);
void Cg_LoadMedia(void);
void Cg_FreeMedia(void);
#endif
