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

SoundSample *cg_sampleBlasterFire;
SoundSample *cg_sampleBlasterHit;
SoundSample *cg_sampleShotgunFire;
SoundSample *cg_sampleSupershotgunFire;
SoundSample *cg_sampleMachinegunFire[3];
SoundSample *cg_sampleMachinegunHit[3];
SoundSample *cg_sampleGrenadelauncherFire;
SoundSample *cg_sampleRocketlauncherFire;
SoundSample *cg_sampleHyperblasterFire;
SoundSample *cg_sampleHyperblasterHit;
SoundSample *cg_sampleLightningFire;
SoundSample *cg_sampleLaserFire;
SoundSample *cg_sampleLightningDischarge;
SoundSample *cg_sampleRailgunFire;
SoundSample *cg_sampleBfgFire;
SoundSample *cg_sampleBfgHit;
#if defined(G_HOOK)
SoundSample *cg_sampleHookHit;
#endif

SoundSample *cg_sampleQuakeShotgunFire;
SoundSample *cg_sampleQuakeSupershotgunFire;
SoundSample *cg_sampleQuakeNailgunFire;
SoundSample *cg_sampleQuakeSupernailgunFire;
SoundSample *cg_sampleQuakeNailHit;
SoundSample *cg_sampleQuakeGrenadelauncherFire;
SoundSample *cg_sampleQuakeRocketlauncherFire;

SoundSample *cg_sampleExplosion;
SoundSample *cg_sampleTeleport;
SoundSample *cg_sampleRespawn;
SoundSample *cg_sampleSparks;
SoundSample *cg_sampleFire;
SoundSample *cg_sampleSteam;

SoundSample *cg_sampleRain;
SoundSample *cg_sampleSnow;
SoundSample *cg_sampleAsh;
SoundSample *cg_sampleUnderwater;
SoundSample *cg_sampleHits[2];
SoundSample *cg_sampleGib;

static RenderAtlas *cg_spriteAtlas;

RenderAtlasImage *cg_spriteParticle;
RenderAtlasImage *cg_spriteParticle2;
RenderAtlasImage *cg_spriteParticle3;
RenderAtlasImage *cg_spriteFlash;
RenderAtlasImage *cg_spriteRing;
RenderAtlasImage *cg_spriteBlasterFlash;
RenderAtlasImage *cg_spriteAnisoFlare01;
RenderAtlasImage *cg_spriteRain;
RenderAtlasImage *cg_spriteSnow;
RenderAtlasImage *cg_spriteAsh;
RenderAtlasImage *cg_spriteBubble;
RenderAtlasImage *cg_spriteTeleport;
RenderAtlasImage *cg_spriteTeleportCore;
RenderAtlasImage *cg_spriteSmoke;
RenderAtlasImage *cg_spriteFlame;
RenderAtlasImage *cg_spriteExplosionGlow;
RenderAtlasImage *cg_spriteExplosionFlash;
RenderAtlasImage *cg_spriteSpark;
RenderAtlasImage *cg_spriteSteam;
RenderAtlasImage *cg_spriteInactive;
RenderAtlasImage *cg_spritePlasmaVar01;
RenderAtlasImage *cg_spritePlasmaVar02;
RenderAtlasImage *cg_spritePlasmaVar03;
RenderAtlasImage *cg_spriteBlob01;
RenderAtlasImage *cg_spriteElectro02;
RenderAtlasImage *cg_spriteSplash0203;
RenderAtlasImage *cg_spriteImpactSpark01Dot;
RenderAtlasImage *cg_spritePuffCloud;
RenderAtlasImage *cg_spriteWaterCircle;
RenderAtlasImage *cg_spriteWaterRing;
RenderAtlasImage *cg_spriteWaterRing2;
RenderAtlasImage *cg_spriteAbstract01;
RenderAtlasImage *cg_spriteNodeWait;
RenderAtlasImage *cg_spriteNodeSlow;

RenderImage *cg_beamHook;
RenderImage *cg_beamArrow;
RenderImage *cg_beamLine;
RenderImage *cg_beamRail;
RenderImage *cg_beamLightning;
RenderImage *cg_beamTracer;
RenderImage *cg_beamTail;

RenderAnimation *cg_spriteExplosion;
RenderAnimation *cg_spriteExplosionRing02;
RenderAnimation *cg_spriteRocketFlame;
RenderAnimation *cg_spriteBlasterFlame;
RenderAnimation *cg_spriteSmoke04;
RenderAnimation *cg_spriteSmoke05;
RenderAnimation *cg_spriteBlasterRing;
RenderAnimation *cg_spriteBfgExplosion1;
RenderAnimation *cg_spriteBfgExplosion2;
RenderAnimation *cg_spriteBfgExplosion3;
RenderAnimation *cg_spritePoof01;
RenderAnimation *cg_spritePoof02;
RenderAnimation *cg_spriteBlood01;
RenderAnimation *cg_spriteElectro01;
RenderAnimation *cg_spriteFireball01;
RenderAnimation *cg_spriteImpactSpark01;
RenderAnimation *cg_spriteHyperball01;
RenderAnimation *cg_spriteFizz01;

static RenderAtlas *cg_decalAtlas;

RenderAtlasImage *cg_decalBullet[3];
RenderAtlasImage *cg_decalBlood[4];
RenderAtlasImage *cg_decalBurn[4];
RenderAtlasImage *cg_decalSlug[4];

Framebuffer *cg_framebuffer;

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
  cg_framebuffer = cgi.CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
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
  
  if (cg_framebuffer) {
    cgi.DestroyFramebuffer(cg_framebuffer);
    cg_framebuffer = NULL;
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

  cg_sampleBlasterFire = cgi.LoadSample("weapons/blaster/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleBlasterHit = cgi.LoadSample("weapons/blaster/hit", ASSET_CONTEXT_SOUNDS);
  cg_sampleShotgunFire = cgi.LoadSample("weapons/shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleSupershotgunFire = cgi.LoadSample("weapons/supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleGrenadelauncherFire = cgi.LoadSample("weapons/grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleRocketlauncherFire = cgi.LoadSample("weapons/rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleHyperblasterFire = cgi.LoadSample("weapons/hyperblaster/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleHyperblasterHit = cgi.LoadSample("weapons/hyperblaster/hit", ASSET_CONTEXT_SOUNDS);
  cg_sampleLightningFire = cgi.LoadSample("weapons/lightning/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleLaserFire = cgi.LoadSample("trigger/laser/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleLightningDischarge = cgi.LoadSample("weapons/lightning/discharge", ASSET_CONTEXT_SOUNDS);
  cg_sampleRailgunFire = cgi.LoadSample("weapons/railgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleBfgFire = cgi.LoadSample("weapons/bfg/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleBfgHit = cgi.LoadSample("weapons/bfg/hit", ASSET_CONTEXT_SOUNDS);

#if defined(G_HOOK)
  cg_sampleHookHit = cgi.LoadSample("grapplehook/hit", ASSET_CONTEXT_SOUNDS);
#endif

  cg_sampleQuakeShotgunFire = cgi.LoadSample("weapons/quake_shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeSupershotgunFire = cgi.LoadSample("weapons/quake_supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeNailgunFire = cgi.LoadSample("weapons/quake_nailgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeSupernailgunFire = cgi.LoadSample("weapons/quake_supernailgun/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeNailHit = cgi.LoadSample("projectiles/quake_nail/hit", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeGrenadelauncherFire = cgi.LoadSample("weapons/quake_grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cg_sampleQuakeRocketlauncherFire = cgi.LoadSample("weapons/quake_rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);

  cg_sampleExplosion = cgi.LoadSample("weapons/common/explosion", ASSET_CONTEXT_SOUNDS);
  cg_sampleTeleport = cgi.LoadSample("misc/teleport", ASSET_CONTEXT_SOUNDS);
  cg_sampleRespawn = cgi.LoadSample("misc/respawn", ASSET_CONTEXT_SOUNDS);
  cg_sampleSparks = cgi.LoadSample("ambient/sparks", ASSET_CONTEXT_SOUNDS);
  cg_sampleFire = cgi.LoadSample("ambient/fire_1", ASSET_CONTEXT_SOUNDS);
  cg_sampleSteam = cgi.LoadSample("ambient/steam_1", ASSET_CONTEXT_SOUNDS);
  cg_sampleRain = cgi.LoadSample("ambient/rain", ASSET_CONTEXT_SOUNDS);
  cg_sampleSnow = cgi.LoadSample("ambient/snow", ASSET_CONTEXT_SOUNDS);
  cg_sampleAsh = cgi.LoadSample("ambient/ash", ASSET_CONTEXT_SOUNDS);
  cg_sampleUnderwater = cgi.LoadSample("ambient/underwater", ASSET_CONTEXT_SOUNDS);
  cg_sampleGib = cgi.LoadSample("gibs/common/gib", ASSET_CONTEXT_SOUNDS);

  for (uint32_t i = 0; i < lengthof(cg_sampleHits); i++) {
    q_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
    cg_sampleHits[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cg_sampleMachinegunFire); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
    cg_sampleMachinegunFire[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cg_sampleMachinegunHit); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
    cg_sampleMachinegunHit[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  cgi.LoadingProgress(-1, "sprites");

  Cg_FreeSprites();

  cg_beamHook = cgi.LoadImage("sprites/rope", IMG_SPRITE);
  cg_beamArrow = cgi.LoadImage("sprites/arrow", IMG_SPRITE);
  cg_beamLine = cgi.LoadImage("sprites/line", IMG_SPRITE);
  cg_beamRail = cgi.LoadImage("sprites/beam", IMG_SPRITE);
  cg_beamLightning = cgi.LoadImage("sprites/lightning", IMG_SPRITE);
  cg_beamTracer = cgi.LoadImage("sprites/tracer", IMG_SPRITE);
  cg_beamTail = cgi.LoadImage("sprites/particle_tail", IMG_SPRITE);

  cg_spriteAtlas = cgi.LoadAtlas("cg_sprite_atlas");
  cg_spriteParticle = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/particle", IMG_SPRITE);
  cg_spriteParticle2 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/particle2", IMG_SPRITE);
  cg_spriteParticle3 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/particle3", IMG_SPRITE);
  cg_spriteFlash = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/flash", IMG_SPRITE);
  cg_spriteRing = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/ring", IMG_SPRITE);
  cg_spriteBlasterFlash = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/blast_01/blast_01_flash", IMG_SPRITE);
  cg_spriteAnisoFlare01 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/aniso_flare_01", IMG_SPRITE);
  cg_spriteSmoke = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/smoke", IMG_SPRITE);
  cg_spriteFlame = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/flame", IMG_SPRITE);
  cg_spriteExplosionGlow = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/explosion_glow", IMG_SPRITE);
  cg_spriteExplosionFlash = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/explosion_flash", IMG_SPRITE);
  cg_spriteSpark = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/spark", IMG_SPRITE);
  cg_spriteRain = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/rain", IMG_SPRITE);
  cg_spriteSnow = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/snow", IMG_SPRITE);
  cg_spriteAsh = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/ash", IMG_SPRITE);
  cg_spriteSteam = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/steam", IMG_SPRITE);
  cg_spriteBubble = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/bubble", IMG_SPRITE);
  cg_spriteInactive = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/inactive", IMG_SPRITE);
  cg_spritePlasmaVar01 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/plasma/plasma_var01", IMG_SPRITE);
  cg_spritePlasmaVar02 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/plasma/plasma_var02", IMG_SPRITE);
  cg_spritePlasmaVar03 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/plasma/plasma_var03", IMG_SPRITE);
  cg_spriteBlob01 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/blob_01", IMG_SPRITE);
  cg_spriteElectro02 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/electro_02/electro_02", IMG_SPRITE);
  cg_spriteTeleport = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/teleport", IMG_SPRITE);
  cg_spriteTeleportCore = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/teleport_core", IMG_SPRITE);
  cg_spriteSplash0203 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/splash_02/splash_02_03", IMG_SPRITE);
  cg_spriteImpactSpark01Dot = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/impact_spark_01/impact_spark_01_dot", IMG_SPRITE);
  cg_spritePuffCloud = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/puff_cloud", IMG_SPRITE);
  cg_spriteWaterCircle = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/water/splash_01_circle", IMG_SPRITE);
  cg_spriteWaterRing = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/water/splash_01_ring", IMG_SPRITE);
  cg_spriteWaterRing2 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/water/splash_01_ring2", IMG_SPRITE);
  cg_spriteAbstract01 = cgi.LoadAtlasImage(cg_spriteAtlas, "sprites/abstract/abstract_01", IMG_SPRITE);
  cg_spriteNodeWait = cgi.LoadAtlasImage(cg_spriteAtlas, "pics/emoji/teamkill", IMG_SPRITE);
  cg_spriteNodeSlow = cgi.LoadAtlasImage(cg_spriteAtlas, "pics/emoji/crush", IMG_SPRITE);

  cg_spriteBlasterRing = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/blast_01/blast_01_ring", "_%02" PRIu32, 1, 7);
  cg_spriteExplosion = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/explosion_01/explosion_01", "_%02" PRIu32, 1, 36);
  cg_spriteExplosionRing02 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/explosion_ring_02/explosion_ring_02", "_%02" PRIu32, 1, 7);
  cg_spriteRocketFlame = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/flame_03/flame_03", "_%02" PRIu32, 1, 29);
  cg_spriteBlasterFlame = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/flame_mono_01/flame_mono_01", "_%02" PRIu32, 1, 21);
  cg_spriteSmoke04 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/smoke_04/smoke_04", "_%02" PRIu32, 1, 90);
  cg_spriteSmoke05 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/smoke_05/smoke_05", "_%02" PRIu32, 1, 99);
  cg_spriteBfgExplosion2 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/bfg_explosion_02/bfg_explosion_02", "_%02" PRIu32, 1, 23);
  cg_spriteBfgExplosion3 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/bfg_explosion_03/bfg_explosion_03", "_%02" PRIu32, 1, 21);
  cg_spritePoof01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/poof_01/poof_01", "_%02" PRIu32, 1, 34);
  cg_spritePoof02 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/poof_02/poof_02", "_%02" PRIu32, 1, 17);
  cg_spriteBlood01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/blood_01/blood_01", "_%02" PRIu32, 1, 10);
  cg_spriteElectro01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/electro_01/electro_01", "_%02" PRIu32, 1, 5);
  cg_spriteFireball01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/fireball_01/fireball_01", "_%02" PRIu32, 0, 63);
  cg_spriteImpactSpark01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/impact_spark_01/impact_spark_01", "_%02" PRIu32, 0, 4);
  cg_spriteHyperball01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/hyperball_01/hyperball_01", "_%02" PRIu32, 1, 32);
  cg_spriteFizz01 = Cg_LoadAnimatedSprite(cg_spriteAtlas, "sprites/fizz_01/fizz_01", "_%02" PRIu32, 1, 24);

  cgi.LoadingProgress(-1, "compiling sprite atlas");

  cgi.CompileAtlas(cg_spriteAtlas);

  cgi.LoadingProgress(-1, "decals");

  cg_decalAtlas = cgi.LoadAtlas("cg_decal_atlas");

  for (size_t i = 0; i < lengthof(cg_decalBullet); i++) {
    q_snprintf(name, sizeof(name), "decals/bullet_%zd", i);
    cg_decalBullet[i] = cgi.LoadAtlasImage(cg_decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cg_decalBlood); i++) {
    q_snprintf(name, sizeof(name), "decals/blood_%zd", i);
    cg_decalBlood[i] = cgi.LoadAtlasImage(cg_decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cg_decalBurn); i++) {
    q_snprintf(name, sizeof(name), "decals/burn_%zd", i);
    cg_decalBurn[i] = cgi.LoadAtlasImage(cg_decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cg_decalSlug); i++) {
      q_snprintf(name, sizeof(name), "decals/slug_%zd", i);
      cg_decalSlug[i] = cgi.LoadAtlasImage(cg_decalAtlas, name, IMG_SPRITE);
  }

  cgi.LoadingProgress(-1, "compiling decal atlas");

  cgi.CompileAtlas(cg_decalAtlas);

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
