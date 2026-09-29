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
extern SoundSample *cgameSampleBlasterFire;
extern SoundSample *cgameSampleBlasterHit;
extern SoundSample *cgameSampleShotgunFire;
extern SoundSample *cgameSampleSupershotgunFire;
extern SoundSample *cgameSampleMachinegunFire[3];
extern SoundSample *cgameSampleMachinegunHit[3];
extern SoundSample *cgameSampleGrenadelauncherFire;
extern SoundSample *cgameSampleRocketlauncherFire;
extern SoundSample *cgameSampleHyperblasterFire;
extern SoundSample *cgameSampleHyperblasterHit;
extern SoundSample *cgameSampleLightningFire;
extern SoundSample *cgameSampleLaserFire;
extern SoundSample *cgameSampleLightningDischarge;
extern SoundSample *cgameSampleRailgunFire;
extern SoundSample *cgameSampleBfgFire;
extern SoundSample *cgameSampleBfgHit;

#if defined(G_HOOK)
extern SoundSample *cgameSampleHookHit;
#endif

extern SoundSample *cgameSampleQuakeShotgunFire;
extern SoundSample *cgameSampleQuakeSupershotgunFire;
extern SoundSample *cgameSampleQuakeNailgunFire;
extern SoundSample *cgameSampleQuakeSupernailgunFire;
extern SoundSample *cgameSampleQuakeNailHit;
extern SoundSample *cgameSampleQuakeGrenadelauncherFire;
extern SoundSample *cgameSampleQuakeRocketlauncherFire;

extern SoundSample *cgameSampleExplosion;
extern SoundSample *cgameSampleTeleport;
extern SoundSample *cgameSampleRespawn;
extern SoundSample *cgameSampleSparks;
extern SoundSample *cgameSampleFire;
extern SoundSample *cgameSampleSteam;

extern SoundSample *cgameSampleRain;
extern SoundSample *cgameSampleSnow;
extern SoundSample *cgameSampleAsh;
extern SoundSample *cgameSampleUnderwater;
extern SoundSample *cgameSampleHits[2];
extern SoundSample *cgameSampleGib;

extern RenderAtlasImage *cgameSpriteParticle;
extern RenderAtlasImage *cgameSpriteParticle2;
extern RenderAtlasImage *cgameSpriteParticle3;
extern RenderAtlasImage *cgameSpriteFlash;
extern RenderAtlasImage *cgameSpriteRing;
extern RenderAtlasImage *cgameSpriteBlasterFlash;
extern RenderAtlasImage *cgameSpriteAnisoFlare01;
extern RenderAtlasImage *cgameSpriteRain;
extern RenderAtlasImage *cgameSpriteSnow;
extern RenderAtlasImage *cgameSpriteAsh;
extern RenderAtlasImage *cgameSpriteSmoke;
extern RenderAtlasImage *cgameSpriteFlame;
extern RenderAtlasImage *cgameSpriteSpark;
extern RenderAtlasImage *cgameSpriteBubble;
extern RenderAtlasImage *cgameSpriteTeleport;
extern RenderAtlasImage *cgameSpriteTeleportCore;
extern RenderAtlasImage *cgameSpriteSteam;
extern RenderAtlasImage *cgameSpriteInactive;
extern RenderAtlasImage *cgameSpritePlasmaVar01;
extern RenderAtlasImage *cgameSpritePlasmaVar02;
extern RenderAtlasImage *cgameSpritePlasmaVar03;
extern RenderAtlasImage *cgameSpriteBlob01;
extern RenderAtlasImage *cgameSpriteElectro02;
extern RenderAtlasImage *cgameSpriteExplosionFlash;
extern RenderAtlasImage *cgameSpriteExplosionGlow;
extern RenderAtlasImage *cgameSpriteSplash0203;
extern RenderAtlasImage *cgameSpriteImpactSpark01Dot;
extern RenderAtlasImage *cgameSpritePuffCloud;
extern RenderAtlasImage *cgameSpriteWaterCircle;
extern RenderAtlasImage *cgameSpriteWaterRing;
extern RenderAtlasImage *cgameSpriteWaterRing2;
extern RenderAtlasImage *cgameSpriteAbstract01;
extern RenderAtlasImage *cgameSpriteNodeWait;
extern RenderAtlasImage *cgameSpriteNodeSlow;

extern RenderImage *cgameBeamHook;
extern RenderImage *cgameBeamArrow;
extern RenderImage *cgameBeamLine;
extern RenderImage *cgameBeamRail;
extern RenderImage *cgameBeamLightning;
extern RenderImage *cgameBeamTracer;
extern RenderImage *cgameBeamTail;

extern RenderAnimation *cgameSpriteExplosion;
extern RenderAnimation *cgameSpriteExplosionRing02;
extern RenderAnimation *cgameSpriteRocketFlame;
extern RenderAnimation *cgameSpriteBlasterFlame;
extern RenderAnimation *cgameSpriteSmoke04;
extern RenderAnimation *cgameSpriteSmoke05;
extern RenderAnimation *cgameSpriteBlasterRing;
extern RenderAnimation *cgameSpriteBfgExplosion1;
extern RenderAnimation *cgameSpriteBfgExplosion2;
extern RenderAnimation *cgameSpriteBfgExplosion3;
extern RenderAnimation *cgameSpritePoof01;
extern RenderAnimation *cgameSpritePoof02;
extern RenderAnimation *cgameSpriteBlood01;
extern RenderAnimation *cgameSpriteElectro01;
extern RenderAnimation *cgameSpriteFireball01;
extern RenderAnimation *cgameSpriteImpactSpark01;
extern RenderAnimation *cgameSpriteHyperball01;
extern RenderAnimation *cgameSpriteFizz01;

extern RenderAtlasImage *cgameDecalBullet[3];
extern RenderAtlasImage *cgameDecalBlood[4];
extern RenderAtlasImage *cgameDecalBurn[4];
extern RenderAtlasImage *cgameDecalSlug[4];

extern Framebuffer *cgameFramebuffer;

void Cg_CreateFramebuffer(void);
void Cg_DestroyFramebuffer(void);
void Cg_LoadMedia(void);
void Cg_FreeMedia(void);
#endif
