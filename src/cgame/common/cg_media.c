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

#include "cg_local.h"

SoundSample *cgSampleBlasterFire;
SoundSample *cgSampleBlasterHit;
SoundSample *cgSampleShotgunFire;
SoundSample *cgSampleSupershotgunFire;
SoundSample *cgSampleMachinegunFire[3];
SoundSample *cgSampleMachinegunHit[3];
SoundSample *cgSampleGrenadelauncherFire;
SoundSample *cgSampleRocketlauncherFire;
SoundSample *cgSampleHyperblasterFire;
SoundSample *cgSampleHyperblasterHit;
SoundSample *cgSampleLightningFire;
SoundSample *cgSampleLaserFire;
SoundSample *cgSampleLightningDischarge;
SoundSample *cgSampleRailgunFire;
SoundSample *cgSampleBfgFire;
SoundSample *cgSampleBfgHit;
#if defined(G_HOOK)
SoundSample *cgSampleHookHit;
#endif

SoundSample *cgSampleQuakeShotgunFire;
SoundSample *cgSampleQuakeSupershotgunFire;
SoundSample *cgSampleQuakeNailgunFire;
SoundSample *cgSampleQuakeSupernailgunFire;
SoundSample *cgSampleQuakeNailHit;
SoundSample *cgSampleQuakeGrenadelauncherFire;
SoundSample *cgSampleQuakeRocketlauncherFire;

SoundSample *cgSampleExplosion;
SoundSample *cgSampleTeleport;
SoundSample *cgSampleRespawn;
SoundSample *cgSampleSparks;
SoundSample *cgSampleFire;
SoundSample *cgSampleSteam;

SoundSample *cgSampleRain;
SoundSample *cgSampleSnow;
SoundSample* cgSampleAsh;
SoundSample *cgSampleUnderwater;
SoundSample *cgSampleHits[2];
SoundSample *cgSampleGib;

static RenderAtlas *cgSpriteAtlas;

RenderAtlasImage *cgSpriteParticle;
RenderAtlasImage *cgSpriteParticle2;
RenderAtlasImage *cgSpriteParticle3;
RenderAtlasImage *cgSpriteFlash;
RenderAtlasImage *cgSpriteRing;
RenderAtlasImage *cgSpriteBlasterFlash;
RenderAtlasImage *cgSpriteAnisoFlare01;
RenderAtlasImage *cgSpriteRain;
RenderAtlasImage *cgSpriteSnow;
RenderAtlasImage* cgSpriteAsh;
RenderAtlasImage *cgSpriteBubble;
RenderAtlasImage *cgSpriteTeleport;
RenderAtlasImage *cgSpriteTeleportCore;
RenderAtlasImage *cgSpriteSmoke;
RenderAtlasImage *cgSpriteFlame;
RenderAtlasImage *cgSpriteExplosionGlow;
RenderAtlasImage *cgSpriteExplosionFlash;
RenderAtlasImage *cgSpriteSpark;
RenderAtlasImage *cgSpriteSteam;
RenderAtlasImage *cgSpriteInactive;
RenderAtlasImage *cgSpritePlasmaVar01;
RenderAtlasImage *cgSpritePlasmaVar02;
RenderAtlasImage *cgSpritePlasmaVar03;
RenderAtlasImage *cgSpriteBlob01;
RenderAtlasImage *cgSpriteElectro02;
RenderAtlasImage *cgSpriteSplash0203;
RenderAtlasImage *cgSpriteImpactSpark01Dot;
RenderAtlasImage *cgSpritePuffCloud;
RenderAtlasImage *cgSpriteWaterCircle;
RenderAtlasImage *cgSpriteWaterRing;
RenderAtlasImage *cgSpriteWaterRing2;
RenderAtlasImage *cgSpriteAbstract01;
RenderAtlasImage *cgSpriteNodeWait;
RenderAtlasImage *cgSpriteNodeSlow;

RenderImage *cgBeamHook;
RenderImage *cgBeamArrow;
RenderImage *cgBeamLine;
RenderImage *cgBeamRail;
RenderImage *cgBeamLightning;
RenderImage *cgBeamTracer;
RenderImage *cgBeamTail;

RenderAnimation *cgSpriteExplosion;
RenderAnimation *cgSpriteExplosionRing02;
RenderAnimation *cgSpriteRocketFlame;
RenderAnimation *cgSpriteBlasterFlame;
RenderAnimation *cgSpriteSmoke04;
RenderAnimation *cgSpriteSmoke05;
RenderAnimation *cgSpriteBlasterRing;
RenderAnimation *cgBfgExplosion1;
RenderAnimation *cgSpriteBfgExplosion2;
RenderAnimation *cgSpriteBfgExplosion3;
RenderAnimation *cgSpritePoof01;
RenderAnimation *cgSpritePoof02;
RenderAnimation *cgSpriteBlood01;
RenderAnimation *cgSpriteElectro01;
RenderAnimation *cgSpriteFireball01;
RenderAnimation *cgSpriteImpactSpark01;
RenderAnimation *cgSpriteHyperball01;
RenderAnimation *cgSpriteFizz01;

static RenderAtlas *cgDecalAtlas;

RenderAtlasImage *cgDecalBullet[3];
RenderAtlasImage *cgDecalBlood[4];
RenderAtlasImage *cgDecalBurn[4];
RenderAtlasImage *cgDecalSlug[4];

Framebuffer *cgFramebuffer;

/**
 * @brief Creates the client game framebuffer sized to the current window dimensions.
 */
void Cg_CreateFramebuffer(void) {
  
  Cg_DestroyFramebuffer();

  const SDL_Rect rect = cgi.context->windowBounds;

  // Color 0: the HDR scene, cleared opaque black. Color 1: a float depth copy
  // (gl_FragCoord.z) the sprite pass samples for soft particles -- double
  // buffered so sprites can sample *last* frame's copy in the same pass that
  // writes this frame's, avoiding a same-frame write-then-read hazard.
  cgFramebuffer = cgi.CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
    .size = MakeSize(rect.w, rect.h),
    .colorAttachments = {
      { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .clearColor = { 0.f, 0.f, 0.f, 1.f } },
      { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .clearColor = { 1.f, 1.f, 1.f, 1.f }, .doubleBuffered = true },
    },
    .numColorTargets = 2,
    .depthAttachment = { .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT, .clearDepth = 1.f },
  });
}

/**
 * @brief Destroys the client game framebuffer if it exists.
 */
void Cg_DestroyFramebuffer(void) {
  
  if (cgFramebuffer) {
    cgi.DestroyFramebuffer(cgFramebuffer);
    cgFramebuffer = NULL;
  }
}

/**
 * @brief Loads a numbered atlas image sequence and returns it as an animation.
 */
static RenderAnimation *Cg_LoadAnimatedSprite(RenderAtlas *atlas, char *basePath, char *seqNumFmt, uint32_t firstFrame, uint32_t lastFrame) {
  assert(lastFrame > firstFrame);

  char formatPath[MAX_QPATH];
  q_snprintf(formatPath, sizeof(formatPath), "%s%s", basePath, seqNumFmt);

  char name[MAX_QPATH];
  const uint32_t length = (lastFrame - firstFrame) + 1;
  const RenderImage *images[length];
  for (uint32_t i = 0; i < length; i++) {
    q_snprintf(name, MAX_QPATH, formatPath, i + firstFrame);
    images[i] = (RenderImage *) cgi.LoadAtlasImage(atlas, name, IMG_SPRITE);
  }

  return cgi.CreateAnimation(basePath, length, images);
}

/**
 * @brief Updates all media references for the client game.
 */
void Cg_LoadMedia(void) {
  char name[MAX_QPATH];

  cgi.FreeTag(MEM_TAG_CGAME);
  cgi.FreeTag(MEM_TAG_CGAME_LEVEL);
  
  cgi.LoadingProgress(-1, "sounds");

  cgSampleBlasterFire = cgi.LoadSample("weapons/blaster/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleBlasterHit = cgi.LoadSample("weapons/blaster/hit", ASSET_CONTEXT_SOUNDS);
  cgSampleShotgunFire = cgi.LoadSample("weapons/shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleSupershotgunFire = cgi.LoadSample("weapons/supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleGrenadelauncherFire = cgi.LoadSample("weapons/grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleRocketlauncherFire = cgi.LoadSample("weapons/rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleHyperblasterFire = cgi.LoadSample("weapons/hyperblaster/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleHyperblasterHit = cgi.LoadSample("weapons/hyperblaster/hit", ASSET_CONTEXT_SOUNDS);
  cgSampleLightningFire = cgi.LoadSample("weapons/lightning/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleLaserFire = cgi.LoadSample("trigger/laser/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleLightningDischarge = cgi.LoadSample("weapons/lightning/discharge", ASSET_CONTEXT_SOUNDS);
  cgSampleRailgunFire = cgi.LoadSample("weapons/railgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleBfgFire = cgi.LoadSample("weapons/bfg/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleBfgHit = cgi.LoadSample("weapons/bfg/hit", ASSET_CONTEXT_SOUNDS);

#if defined(G_HOOK)
  cgSampleHookHit = cgi.LoadSample("grapplehook/hit", ASSET_CONTEXT_SOUNDS);
#endif

  cgSampleQuakeShotgunFire = cgi.LoadSample("weapons/quake_shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeSupershotgunFire = cgi.LoadSample("weapons/quake_supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeNailgunFire = cgi.LoadSample("weapons/quake_nailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeSupernailgunFire = cgi.LoadSample("weapons/quake_supernailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeNailHit = cgi.LoadSample("projectiles/quake_nail/hit", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeGrenadelauncherFire = cgi.LoadSample("weapons/quake_grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgSampleQuakeRocketlauncherFire = cgi.LoadSample("weapons/quake_rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);

  cgSampleExplosion = cgi.LoadSample("weapons/common/explosion", ASSET_CONTEXT_SOUNDS);
  cgSampleTeleport = cgi.LoadSample("misc/teleport", ASSET_CONTEXT_SOUNDS);
  cgSampleRespawn = cgi.LoadSample("misc/respawn", ASSET_CONTEXT_SOUNDS);
  cgSampleSparks = cgi.LoadSample("ambient/sparks", ASSET_CONTEXT_SOUNDS);
  cgSampleFire = cgi.LoadSample("ambient/fire_1", ASSET_CONTEXT_SOUNDS);
  cgSampleSteam = cgi.LoadSample("ambient/steam_1", ASSET_CONTEXT_SOUNDS);
  cgSampleRain = cgi.LoadSample("ambient/rain", ASSET_CONTEXT_SOUNDS);
  cgSampleSnow = cgi.LoadSample("ambient/snow", ASSET_CONTEXT_SOUNDS);
  cgSampleAsh = cgi.LoadSample("ambient/ash", ASSET_CONTEXT_SOUNDS);
  cgSampleUnderwater = cgi.LoadSample("ambient/underwater", ASSET_CONTEXT_SOUNDS);
  cgSampleGib = cgi.LoadSample("gibs/common/gib", ASSET_CONTEXT_SOUNDS);

  for (uint32_t i = 0; i < lengthof(cgSampleHits); i++) {
    q_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
    cgSampleHits[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgSampleMachinegunFire); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
    cgSampleMachinegunFire[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgSampleMachinegunHit); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
    cgSampleMachinegunHit[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  cgi.LoadingProgress(-1, "sprites");

  Cg_FreeSprites();

  cgBeamHook = cgi.LoadImage("sprites/rope", IMG_SPRITE);
  cgBeamArrow = cgi.LoadImage("sprites/arrow", IMG_SPRITE);
  cgBeamLine = cgi.LoadImage("sprites/line", IMG_SPRITE);
  cgBeamRail = cgi.LoadImage("sprites/beam", IMG_SPRITE);
  cgBeamLightning = cgi.LoadImage("sprites/lightning", IMG_SPRITE);
  cgBeamTracer = cgi.LoadImage("sprites/tracer", IMG_SPRITE);
  cgBeamTail = cgi.LoadImage("sprites/particle_tail", IMG_SPRITE);

  cgSpriteAtlas = cgi.LoadAtlas("cg_sprite_atlas");
  cgSpriteParticle = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/particle", IMG_SPRITE);
  cgSpriteParticle2 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/particle2", IMG_SPRITE);
  cgSpriteParticle3 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/particle3", IMG_SPRITE);
  cgSpriteFlash = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/flash", IMG_SPRITE);
  cgSpriteRing = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/ring", IMG_SPRITE);
  cgSpriteBlasterFlash = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/blast_01/blast_01_flash", IMG_SPRITE);
  cgSpriteAnisoFlare01 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/aniso_flare_01", IMG_SPRITE);
  cgSpriteSmoke = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/smoke", IMG_SPRITE);
  cgSpriteFlame = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/flame", IMG_SPRITE);
  cgSpriteExplosionGlow = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/explosion_glow", IMG_SPRITE);
  cgSpriteExplosionFlash = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/explosion_flash", IMG_SPRITE);
  cgSpriteSpark = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/spark", IMG_SPRITE);
  cgSpriteRain = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/rain", IMG_SPRITE);
  cgSpriteSnow = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/snow", IMG_SPRITE);
  cgSpriteAsh = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/ash", IMG_SPRITE);
  cgSpriteSteam = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/steam", IMG_SPRITE);
  cgSpriteBubble = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/bubble", IMG_SPRITE);
  cgSpriteInactive = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/inactive", IMG_SPRITE);
  cgSpritePlasmaVar01 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/plasma/plasma_var01", IMG_SPRITE);
  cgSpritePlasmaVar02 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/plasma/plasma_var02", IMG_SPRITE);
  cgSpritePlasmaVar03 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/plasma/plasma_var03", IMG_SPRITE);
  cgSpriteBlob01 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/blob_01", IMG_SPRITE);
  cgSpriteElectro02 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/electro_02/electro_02", IMG_SPRITE);
  cgSpriteTeleport = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/teleport", IMG_SPRITE);
  cgSpriteTeleportCore = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/teleport_core", IMG_SPRITE);
  cgSpriteSplash0203 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/splash_02/splash_02_03", IMG_SPRITE);
  cgSpriteImpactSpark01Dot = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/impact_spark_01/impact_spark_01_dot", IMG_SPRITE);
  cgSpritePuffCloud = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/puff_cloud", IMG_SPRITE);
  cgSpriteWaterCircle = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/water/splash_01_circle", IMG_SPRITE);
  cgSpriteWaterRing = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/water/splash_01_ring", IMG_SPRITE);
  cgSpriteWaterRing2 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/water/splash_01_ring2", IMG_SPRITE);
  cgSpriteAbstract01 = cgi.LoadAtlasImage(cgSpriteAtlas, "sprites/abstract/abstract_01", IMG_SPRITE);
  cgSpriteNodeWait = cgi.LoadAtlasImage(cgSpriteAtlas, "pics/emoji/teamkill", IMG_SPRITE);
  cgSpriteNodeSlow = cgi.LoadAtlasImage(cgSpriteAtlas, "pics/emoji/crush", IMG_SPRITE);

  cgSpriteBlasterRing = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/blast_01/blast_01_ring", "_%02" PRIu32, 1, 7);
  cgSpriteExplosion = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/explosion_01/explosion_01", "_%02" PRIu32, 1, 36);
  cgSpriteExplosionRing02 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/explosion_ring_02/explosion_ring_02", "_%02" PRIu32, 1, 7);
  cgSpriteRocketFlame = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/flame_03/flame_03", "_%02" PRIu32, 1, 29);
  cgSpriteBlasterFlame = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/flame_mono_01/flame_mono_01", "_%02" PRIu32, 1, 21);
  cgSpriteSmoke04 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/smoke_04/smoke_04", "_%02" PRIu32, 1, 90);
  cgSpriteSmoke05 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/smoke_05/smoke_05", "_%02" PRIu32, 1, 99);
  cgSpriteBfgExplosion2 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/bfg_explosion_02/bfg_explosion_02", "_%02" PRIu32, 1, 23);
  cgSpriteBfgExplosion3 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/bfg_explosion_03/bfg_explosion_03", "_%02" PRIu32, 1, 21);
  cgSpritePoof01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/poof_01/poof_01", "_%02" PRIu32, 1, 34);
  cgSpritePoof02 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/poof_02/poof_02", "_%02" PRIu32, 1, 17);
  cgSpriteBlood01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/blood_01/blood_01", "_%02" PRIu32, 1, 10);
  cgSpriteElectro01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/electro_01/electro_01", "_%02" PRIu32, 1, 5);
  cgSpriteFireball01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/fireball_01/fireball_01", "_%02" PRIu32, 0, 63);
  cgSpriteImpactSpark01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/impact_spark_01/impact_spark_01", "_%02" PRIu32, 0, 4);
  cgSpriteHyperball01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/hyperball_01/hyperball_01", "_%02" PRIu32, 1, 32);
  cgSpriteFizz01 = Cg_LoadAnimatedSprite(cgSpriteAtlas, "sprites/fizz_01/fizz_01", "_%02" PRIu32, 1, 24);

  cgi.LoadingProgress(-1, "compiling sprite atlas");

  cgi.CompileAtlas(cgSpriteAtlas);

  cgi.LoadingProgress(-1, "decals");

  cgDecalAtlas = cgi.LoadAtlas("cg_decal_atlas");

  for (size_t i = 0; i < lengthof(cgDecalBullet); i++) {
    q_snprintf(name, sizeof(name), "decals/bullet_%zd", i);
    cgDecalBullet[i] = cgi.LoadAtlasImage(cgDecalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgDecalBlood); i++) {
    q_snprintf(name, sizeof(name), "decals/blood_%zd", i);
    cgDecalBlood[i] = cgi.LoadAtlasImage(cgDecalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgDecalBurn); i++) {
    q_snprintf(name, sizeof(name), "decals/burn_%zd", i);
    cgDecalBurn[i] = cgi.LoadAtlasImage(cgDecalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgDecalSlug); i++) {
      q_snprintf(name, sizeof(name), "decals/slug_%zd", i);
      cgDecalSlug[i] = cgi.LoadAtlasImage(cgDecalAtlas, name, IMG_SPRITE);
  }

  cgi.LoadingProgress(-1, "compiling decal atlas");

  cgi.CompileAtlas(cgDecalAtlas);

  Cg_LoadFlares();

  cgi.LoadingProgress(-1, "entities");

  Cg_LoadEntities();

  Cg_LoadEditorEntities();

  Cg_InitLights();

  cgi.LoadingProgress(-1, "clients");

  Cg_LoadClients();

  cgi.LoadingProgress(-1, "hud");

  cg_drawCrosshair->modified = true;
  cg_drawCrosshairColor->modified = true;

  Cg_LoadHudMedia();
  
  Cg_CreateFramebuffer();

  Cg_MediaDidLoad();

  Cg_Debug("Complete\n");
}

static void Cg_MediaDidLoad_Common(void) {
}

MediaDidLoad Cg_MediaDidLoad = Cg_MediaDidLoad_Common;

/**
 * @brief Frees all client game media and resets per-level subsystem state.
 */
void Cg_FreeMedia(void) {

  Cg_DestroyFramebuffer();

  Cg_FreeLights();

  Cg_FreeEntities();

  Cg_FreeEditorEntities();

  Cg_FreeFlares();

  Cg_FreeSprites();

  cgi.FreeTag(MEM_TAG_CGAME);
  cgi.FreeTag(MEM_TAG_CGAME_LEVEL);
}
