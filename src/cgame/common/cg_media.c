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

SoundSample *cgameSampleBlasterFire;
SoundSample *cgameSampleBlasterHit;
SoundSample *cgameSampleShotgunFire;
SoundSample *cgameSampleSupershotgunFire;
SoundSample *cgameSampleMachinegunFire[3];
SoundSample *cgameSampleMachinegunHit[3];
SoundSample *cgameSampleGrenadelauncherFire;
SoundSample *cgameSampleRocketlauncherFire;
SoundSample *cgameSampleHyperblasterFire;
SoundSample *cgameSampleHyperblasterHit;
SoundSample *cgameSampleLightningFire;
SoundSample *cgameSampleLaserFire;
SoundSample *cgameSampleLightningDischarge;
SoundSample *cgameSampleRailgunFire;
SoundSample *cgameSampleBfgFire;
SoundSample *cgameSampleBfgHit;
#if defined(G_HOOK)
SoundSample *cgameSampleHookHit;
#endif

SoundSample *cgameSampleQuakeShotgunFire;
SoundSample *cgameSampleQuakeSupershotgunFire;
SoundSample *cgameSampleQuakeNailgunFire;
SoundSample *cgameSampleQuakeSupernailgunFire;
SoundSample *cgameSampleQuakeNailHit;
SoundSample *cgameSampleQuakeGrenadelauncherFire;
SoundSample *cgameSampleQuakeRocketlauncherFire;

SoundSample *cgameSampleExplosion;
SoundSample *cgameSampleTeleport;
SoundSample *cgameSampleRespawn;
SoundSample *cgameSampleSparks;
SoundSample *cgameSampleFire;
SoundSample *cgameSampleSteam;

SoundSample *cgameSampleRain;
SoundSample *cgameSampleSnow;
SoundSample *cgameSampleAsh;
SoundSample *cgameSampleUnderwater;
SoundSample *cgameSampleHits[2];
SoundSample *cgameSampleGib;

static RenderAtlas *spriteAtlas;

RenderAtlasImage *cgameSpriteParticle;
RenderAtlasImage *cgameSpriteParticle2;
RenderAtlasImage *cgameSpriteParticle3;
RenderAtlasImage *cgameSpriteFlash;
RenderAtlasImage *cgameSpriteRing;
RenderAtlasImage *cgameSpriteBlasterFlash;
RenderAtlasImage *cgameSpriteAnisoFlare01;
RenderAtlasImage *cgameSpriteRain;
RenderAtlasImage *cgameSpriteSnow;
RenderAtlasImage *cgameSpriteAsh;
RenderAtlasImage *cgameSpriteBubble;
RenderAtlasImage *cgameSpriteTeleport;
RenderAtlasImage *cgameSpriteTeleportCore;
RenderAtlasImage *cgameSpriteSmoke;
RenderAtlasImage *cgameSpriteFlame;
RenderAtlasImage *cgameSpriteExplosionGlow;
RenderAtlasImage *cgameSpriteExplosionFlash;
RenderAtlasImage *cgameSpriteSpark;
RenderAtlasImage *cgameSpriteSteam;
RenderAtlasImage *cgameSpriteInactive;
RenderAtlasImage *cgameSpritePlasmaVar01;
RenderAtlasImage *cgameSpritePlasmaVar02;
RenderAtlasImage *cgameSpritePlasmaVar03;
RenderAtlasImage *cgameSpriteBlob01;
RenderAtlasImage *cgameSpriteElectro02;
RenderAtlasImage *cgameSpriteSplash0203;
RenderAtlasImage *cgameSpriteImpactSpark01Dot;
RenderAtlasImage *cgameSpritePuffCloud;
RenderAtlasImage *cgameSpriteWaterCircle;
RenderAtlasImage *cgameSpriteWaterRing;
RenderAtlasImage *cgameSpriteWaterRing2;
RenderAtlasImage *cgameSpriteAbstract01;
RenderAtlasImage *cgameSpriteNodeWait;
RenderAtlasImage *cgameSpriteNodeSlow;

RenderImage *cgameBeamHook;
RenderImage *cgameBeamArrow;
RenderImage *cgameBeamLine;
RenderImage *cgameBeamRail;
RenderImage *cgameBeamLightning;
RenderImage *cgameBeamTracer;
RenderImage *cgameBeamTail;

RenderAnimation *cgameSpriteExplosion;
RenderAnimation *cgameSpriteExplosionRing02;
RenderAnimation *cgameSpriteRocketFlame;
RenderAnimation *cgameSpriteBlasterFlame;
RenderAnimation *cgameSpriteSmoke04;
RenderAnimation *cgameSpriteSmoke05;
RenderAnimation *cgameSpriteBlasterRing;
RenderAnimation *cgameSpriteBfgExplosion1;
RenderAnimation *cgameSpriteBfgExplosion2;
RenderAnimation *cgameSpriteBfgExplosion3;
RenderAnimation *cgameSpritePoof01;
RenderAnimation *cgameSpritePoof02;
RenderAnimation *cgameSpriteBlood01;
RenderAnimation *cgameSpriteElectro01;
RenderAnimation *cgameSpriteFireball01;
RenderAnimation *cgameSpriteImpactSpark01;
RenderAnimation *cgameSpriteHyperball01;
RenderAnimation *cgameSpriteFizz01;

static RenderAtlas *decalAtlas;

RenderAtlasImage *cgameDecalBullet[3];
RenderAtlasImage *cgameDecalBlood[4];
RenderAtlasImage *cgameDecalBurn[4];
RenderAtlasImage *cgameDecalSlug[4];

Framebuffer *cgameFramebuffer;

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
  cgameFramebuffer = cgi.CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
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
  
  if (cgameFramebuffer) {
    cgi.DestroyFramebuffer(cgameFramebuffer);
    cgameFramebuffer = NULL;
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

  cgameSampleBlasterFire = cgi.LoadSample("weapons/blaster/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleBlasterHit = cgi.LoadSample("weapons/blaster/hit", ASSET_CONTEXT_SOUNDS);
  cgameSampleShotgunFire = cgi.LoadSample("weapons/shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleSupershotgunFire = cgi.LoadSample("weapons/supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleGrenadelauncherFire = cgi.LoadSample("weapons/grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleRocketlauncherFire = cgi.LoadSample("weapons/rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleHyperblasterFire = cgi.LoadSample("weapons/hyperblaster/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleHyperblasterHit = cgi.LoadSample("weapons/hyperblaster/hit", ASSET_CONTEXT_SOUNDS);
  cgameSampleLightningFire = cgi.LoadSample("weapons/lightning/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleLaserFire = cgi.LoadSample("trigger/laser/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleLightningDischarge = cgi.LoadSample("weapons/lightning/discharge", ASSET_CONTEXT_SOUNDS);
  cgameSampleRailgunFire = cgi.LoadSample("weapons/railgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleBfgFire = cgi.LoadSample("weapons/bfg/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleBfgHit = cgi.LoadSample("weapons/bfg/hit", ASSET_CONTEXT_SOUNDS);

#if defined(G_HOOK)
  cgameSampleHookHit = cgi.LoadSample("grapplehook/hit", ASSET_CONTEXT_SOUNDS);
#endif

  cgameSampleQuakeShotgunFire = cgi.LoadSample("weapons/quake_shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeSupershotgunFire = cgi.LoadSample("weapons/quake_supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeNailgunFire = cgi.LoadSample("weapons/quake_nailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeSupernailgunFire = cgi.LoadSample("weapons/quake_supernailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeNailHit = cgi.LoadSample("projectiles/quake_nail/hit", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeGrenadelauncherFire = cgi.LoadSample("weapons/quake_grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameSampleQuakeRocketlauncherFire = cgi.LoadSample("weapons/quake_rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);

  cgameSampleExplosion = cgi.LoadSample("weapons/common/explosion", ASSET_CONTEXT_SOUNDS);
  cgameSampleTeleport = cgi.LoadSample("misc/teleport", ASSET_CONTEXT_SOUNDS);
  cgameSampleRespawn = cgi.LoadSample("misc/respawn", ASSET_CONTEXT_SOUNDS);
  cgameSampleSparks = cgi.LoadSample("ambient/sparks", ASSET_CONTEXT_SOUNDS);
  cgameSampleFire = cgi.LoadSample("ambient/fire_1", ASSET_CONTEXT_SOUNDS);
  cgameSampleSteam = cgi.LoadSample("ambient/steam_1", ASSET_CONTEXT_SOUNDS);
  cgameSampleRain = cgi.LoadSample("ambient/rain", ASSET_CONTEXT_SOUNDS);
  cgameSampleSnow = cgi.LoadSample("ambient/snow", ASSET_CONTEXT_SOUNDS);
  cgameSampleAsh = cgi.LoadSample("ambient/ash", ASSET_CONTEXT_SOUNDS);
  cgameSampleUnderwater = cgi.LoadSample("ambient/underwater", ASSET_CONTEXT_SOUNDS);
  cgameSampleGib = cgi.LoadSample("gibs/common/gib", ASSET_CONTEXT_SOUNDS);

  for (uint32_t i = 0; i < lengthof(cgameSampleHits); i++) {
    q_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
    cgameSampleHits[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgameSampleMachinegunFire); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
    cgameSampleMachinegunFire[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgameSampleMachinegunHit); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
    cgameSampleMachinegunHit[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  cgi.LoadingProgress(-1, "sprites");

  Cg_FreeSprites();

  cgameBeamHook = cgi.LoadImage("sprites/rope", IMG_SPRITE);
  cgameBeamArrow = cgi.LoadImage("sprites/arrow", IMG_SPRITE);
  cgameBeamLine = cgi.LoadImage("sprites/line", IMG_SPRITE);
  cgameBeamRail = cgi.LoadImage("sprites/beam", IMG_SPRITE);
  cgameBeamLightning = cgi.LoadImage("sprites/lightning", IMG_SPRITE);
  cgameBeamTracer = cgi.LoadImage("sprites/tracer", IMG_SPRITE);
  cgameBeamTail = cgi.LoadImage("sprites/particle_tail", IMG_SPRITE);

  spriteAtlas = cgi.LoadAtlas("cg_sprite_atlas");
  cgameSpriteParticle = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle", IMG_SPRITE);
  cgameSpriteParticle2 = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle2", IMG_SPRITE);
  cgameSpriteParticle3 = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle3", IMG_SPRITE);
  cgameSpriteFlash = cgi.LoadAtlasImage(spriteAtlas, "sprites/flash", IMG_SPRITE);
  cgameSpriteRing = cgi.LoadAtlasImage(spriteAtlas, "sprites/ring", IMG_SPRITE);
  cgameSpriteBlasterFlash = cgi.LoadAtlasImage(spriteAtlas, "sprites/blast_01/blast_01_flash", IMG_SPRITE);
  cgameSpriteAnisoFlare01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/aniso_flare_01", IMG_SPRITE);
  cgameSpriteSmoke = cgi.LoadAtlasImage(spriteAtlas, "sprites/smoke", IMG_SPRITE);
  cgameSpriteFlame = cgi.LoadAtlasImage(spriteAtlas, "sprites/flame", IMG_SPRITE);
  cgameSpriteExplosionGlow = cgi.LoadAtlasImage(spriteAtlas, "sprites/explosion_glow", IMG_SPRITE);
  cgameSpriteExplosionFlash = cgi.LoadAtlasImage(spriteAtlas, "sprites/explosion_flash", IMG_SPRITE);
  cgameSpriteSpark = cgi.LoadAtlasImage(spriteAtlas, "sprites/spark", IMG_SPRITE);
  cgameSpriteRain = cgi.LoadAtlasImage(spriteAtlas, "sprites/rain", IMG_SPRITE);
  cgameSpriteSnow = cgi.LoadAtlasImage(spriteAtlas, "sprites/snow", IMG_SPRITE);
  cgameSpriteAsh = cgi.LoadAtlasImage(spriteAtlas, "sprites/ash", IMG_SPRITE);
  cgameSpriteSteam = cgi.LoadAtlasImage(spriteAtlas, "sprites/steam", IMG_SPRITE);
  cgameSpriteBubble = cgi.LoadAtlasImage(spriteAtlas, "sprites/bubble", IMG_SPRITE);
  cgameSpriteInactive = cgi.LoadAtlasImage(spriteAtlas, "sprites/inactive", IMG_SPRITE);
  cgameSpritePlasmaVar01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var01", IMG_SPRITE);
  cgameSpritePlasmaVar02 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var02", IMG_SPRITE);
  cgameSpritePlasmaVar03 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var03", IMG_SPRITE);
  cgameSpriteBlob01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/blob_01", IMG_SPRITE);
  cgameSpriteElectro02 = cgi.LoadAtlasImage(spriteAtlas, "sprites/electro_02/electro_02", IMG_SPRITE);
  cgameSpriteTeleport = cgi.LoadAtlasImage(spriteAtlas, "sprites/teleport", IMG_SPRITE);
  cgameSpriteTeleportCore = cgi.LoadAtlasImage(spriteAtlas, "sprites/teleport_core", IMG_SPRITE);
  cgameSpriteSplash0203 = cgi.LoadAtlasImage(spriteAtlas, "sprites/splash_02/splash_02_03", IMG_SPRITE);
  cgameSpriteImpactSpark01Dot = cgi.LoadAtlasImage(spriteAtlas, "sprites/impact_spark_01/impact_spark_01_dot", IMG_SPRITE);
  cgameSpritePuffCloud = cgi.LoadAtlasImage(spriteAtlas, "sprites/puff_cloud", IMG_SPRITE);
  cgameSpriteWaterCircle = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_circle", IMG_SPRITE);
  cgameSpriteWaterRing = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_ring", IMG_SPRITE);
  cgameSpriteWaterRing2 = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_ring2", IMG_SPRITE);
  cgameSpriteAbstract01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/abstract/abstract_01", IMG_SPRITE);
  cgameSpriteNodeWait = cgi.LoadAtlasImage(spriteAtlas, "pics/emoji/teamkill", IMG_SPRITE);
  cgameSpriteNodeSlow = cgi.LoadAtlasImage(spriteAtlas, "pics/emoji/crush", IMG_SPRITE);

  cgameSpriteBlasterRing = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/blast_01/blast_01_ring", "_%02" PRIu32, 1, 7);
  cgameSpriteExplosion = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/explosion_01/explosion_01", "_%02" PRIu32, 1, 36);
  cgameSpriteExplosionRing02 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/explosion_ring_02/explosion_ring_02", "_%02" PRIu32, 1, 7);
  cgameSpriteRocketFlame = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/flame_03/flame_03", "_%02" PRIu32, 1, 29);
  cgameSpriteBlasterFlame = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/flame_mono_01/flame_mono_01", "_%02" PRIu32, 1, 21);
  cgameSpriteSmoke04 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/smoke_04/smoke_04", "_%02" PRIu32, 1, 90);
  cgameSpriteSmoke05 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/smoke_05/smoke_05", "_%02" PRIu32, 1, 99);
  cgameSpriteBfgExplosion2 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/bfg_explosion_02/bfg_explosion_02", "_%02" PRIu32, 1, 23);
  cgameSpriteBfgExplosion3 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/bfg_explosion_03/bfg_explosion_03", "_%02" PRIu32, 1, 21);
  cgameSpritePoof01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/poof_01/poof_01", "_%02" PRIu32, 1, 34);
  cgameSpritePoof02 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/poof_02/poof_02", "_%02" PRIu32, 1, 17);
  cgameSpriteBlood01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/blood_01/blood_01", "_%02" PRIu32, 1, 10);
  cgameSpriteElectro01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/electro_01/electro_01", "_%02" PRIu32, 1, 5);
  cgameSpriteFireball01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/fireball_01/fireball_01", "_%02" PRIu32, 0, 63);
  cgameSpriteImpactSpark01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/impact_spark_01/impact_spark_01", "_%02" PRIu32, 0, 4);
  cgameSpriteHyperball01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/hyperball_01/hyperball_01", "_%02" PRIu32, 1, 32);
  cgameSpriteFizz01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/fizz_01/fizz_01", "_%02" PRIu32, 1, 24);

  cgi.LoadingProgress(-1, "compiling sprite atlas");

  cgi.CompileAtlas(spriteAtlas);

  cgi.LoadingProgress(-1, "decals");

  decalAtlas = cgi.LoadAtlas("cg_decal_atlas");

  for (size_t i = 0; i < lengthof(cgameDecalBullet); i++) {
    q_snprintf(name, sizeof(name), "decals/bullet_%zd", i);
    cgameDecalBullet[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameDecalBlood); i++) {
    q_snprintf(name, sizeof(name), "decals/blood_%zd", i);
    cgameDecalBlood[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameDecalBurn); i++) {
    q_snprintf(name, sizeof(name), "decals/burn_%zd", i);
    cgameDecalBurn[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameDecalSlug); i++) {
      q_snprintf(name, sizeof(name), "decals/slug_%zd", i);
      cgameDecalSlug[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  cgi.LoadingProgress(-1, "compiling decal atlas");

  cgi.CompileAtlas(decalAtlas);

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
