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

CGameMedia cgameMedia;

static RenderAtlas *spriteAtlas;
static RenderAtlas *decalAtlas;

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

  cgameMedia.sounds.blasterFire = cgi.LoadSample("weapons/blaster/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.blasterHit = cgi.LoadSample("weapons/blaster/hit", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.shotgunFire = cgi.LoadSample("weapons/shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.supershotgunFire = cgi.LoadSample("weapons/supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.grenadelauncherFire = cgi.LoadSample("weapons/grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.rocketlauncherFire = cgi.LoadSample("weapons/rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.hyperblasterFire = cgi.LoadSample("weapons/hyperblaster/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.hyperblasterHit = cgi.LoadSample("weapons/hyperblaster/hit", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.lightningFire = cgi.LoadSample("weapons/lightning/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.laserFire = cgi.LoadSample("trigger/laser/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.lightningDischarge = cgi.LoadSample("weapons/lightning/discharge", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.railgunFire = cgi.LoadSample("weapons/railgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.bfgFire = cgi.LoadSample("weapons/bfg/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.bfgHit = cgi.LoadSample("weapons/bfg/hit", ASSET_CONTEXT_SOUNDS);

#if defined(G_HOOK)
  cgameMedia.sounds.hookHit = cgi.LoadSample("grapplehook/hit", ASSET_CONTEXT_SOUNDS);
#endif

  cgameMedia.sounds.quakeShotgunFire = cgi.LoadSample("weapons/quake_shotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeSupershotgunFire = cgi.LoadSample("weapons/quake_supershotgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeNailgunFire = cgi.LoadSample("weapons/quake_nailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeSupernailgunFire = cgi.LoadSample("weapons/quake_supernailgun/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeNailHit = cgi.LoadSample("projectiles/quake_nail/hit", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeGrenadelauncherFire = cgi.LoadSample("weapons/quake_grenadelauncher/fire", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.quakeRocketlauncherFire = cgi.LoadSample("weapons/quake_rocketlauncher/fire", ASSET_CONTEXT_SOUNDS);

  cgameMedia.sounds.explosion = cgi.LoadSample("weapons/common/explosion", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.teleport = cgi.LoadSample("misc/teleport", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.respawn = cgi.LoadSample("misc/respawn", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.sparks = cgi.LoadSample("ambient/sparks", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.fire = cgi.LoadSample("ambient/fire_1", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.steam = cgi.LoadSample("ambient/steam_1", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.rain = cgi.LoadSample("ambient/rain", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.snow = cgi.LoadSample("ambient/snow", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.ash = cgi.LoadSample("ambient/ash", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.underwater = cgi.LoadSample("ambient/underwater", ASSET_CONTEXT_SOUNDS);
  cgameMedia.sounds.gib = cgi.LoadSample("gibs/common/gib", ASSET_CONTEXT_SOUNDS);

  for (uint32_t i = 0; i < lengthof(cgameMedia.sounds.hits); i++) {
    q_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
    cgameMedia.sounds.hits[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgameMedia.sounds.machinegunFire); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
    cgameMedia.sounds.machinegunFire[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  for (uint32_t i = 0; i < lengthof(cgameMedia.sounds.machinegunHit); i++) {
    q_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
    cgameMedia.sounds.machinegunHit[i] = cgi.LoadSample(name, ASSET_CONTEXT_SOUNDS);
  }

  cgi.LoadingProgress(-1, "sprites");

  Cg_FreeSprites();

  cgameMedia.beams.hook = cgi.LoadImage("sprites/rope", IMG_SPRITE);
  cgameMedia.beams.arrow = cgi.LoadImage("sprites/arrow", IMG_SPRITE);
  cgameMedia.beams.line = cgi.LoadImage("sprites/line", IMG_SPRITE);
  cgameMedia.beams.rail = cgi.LoadImage("sprites/beam", IMG_SPRITE);
  cgameMedia.beams.lightning = cgi.LoadImage("sprites/lightning", IMG_SPRITE);
  cgameMedia.beams.tracer = cgi.LoadImage("sprites/tracer", IMG_SPRITE);
  cgameMedia.beams.tail = cgi.LoadImage("sprites/particle_tail", IMG_SPRITE);

  spriteAtlas = cgi.LoadAtlas("cg_sprite_atlas");
  cgameMedia.sprites.particle = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle", IMG_SPRITE);
  cgameMedia.sprites.particle2 = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle2", IMG_SPRITE);
  cgameMedia.sprites.particle3 = cgi.LoadAtlasImage(spriteAtlas, "sprites/particle3", IMG_SPRITE);
  cgameMedia.sprites.flash = cgi.LoadAtlasImage(spriteAtlas, "sprites/flash", IMG_SPRITE);
  cgameMedia.sprites.ring = cgi.LoadAtlasImage(spriteAtlas, "sprites/ring", IMG_SPRITE);
  cgameMedia.sprites.blasterFlash = cgi.LoadAtlasImage(spriteAtlas, "sprites/blast_01/blast_01_flash", IMG_SPRITE);
  cgameMedia.sprites.anisoFlare01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/aniso_flare_01", IMG_SPRITE);
  cgameMedia.sprites.smoke = cgi.LoadAtlasImage(spriteAtlas, "sprites/smoke", IMG_SPRITE);
  cgameMedia.sprites.flame = cgi.LoadAtlasImage(spriteAtlas, "sprites/flame", IMG_SPRITE);
  cgameMedia.sprites.explosionGlow = cgi.LoadAtlasImage(spriteAtlas, "sprites/explosion_glow", IMG_SPRITE);
  cgameMedia.sprites.explosionFlash = cgi.LoadAtlasImage(spriteAtlas, "sprites/explosion_flash", IMG_SPRITE);
  cgameMedia.sprites.spark = cgi.LoadAtlasImage(spriteAtlas, "sprites/spark", IMG_SPRITE);
  cgameMedia.sprites.rain = cgi.LoadAtlasImage(spriteAtlas, "sprites/rain", IMG_SPRITE);
  cgameMedia.sprites.snow = cgi.LoadAtlasImage(spriteAtlas, "sprites/snow", IMG_SPRITE);
  cgameMedia.sprites.ash = cgi.LoadAtlasImage(spriteAtlas, "sprites/ash", IMG_SPRITE);
  cgameMedia.sprites.steam = cgi.LoadAtlasImage(spriteAtlas, "sprites/steam", IMG_SPRITE);
  cgameMedia.sprites.bubble = cgi.LoadAtlasImage(spriteAtlas, "sprites/bubble", IMG_SPRITE);
  cgameMedia.sprites.inactive = cgi.LoadAtlasImage(spriteAtlas, "sprites/inactive", IMG_SPRITE);
  cgameMedia.sprites.plasmaVar01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var01", IMG_SPRITE);
  cgameMedia.sprites.plasmaVar02 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var02", IMG_SPRITE);
  cgameMedia.sprites.plasmaVar03 = cgi.LoadAtlasImage(spriteAtlas, "sprites/plasma/plasma_var03", IMG_SPRITE);
  cgameMedia.sprites.blob01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/blob_01", IMG_SPRITE);
  cgameMedia.sprites.electro02 = cgi.LoadAtlasImage(spriteAtlas, "sprites/electro_02/electro_02", IMG_SPRITE);
  cgameMedia.sprites.teleport = cgi.LoadAtlasImage(spriteAtlas, "sprites/teleport", IMG_SPRITE);
  cgameMedia.sprites.teleportCore = cgi.LoadAtlasImage(spriteAtlas, "sprites/teleport_core", IMG_SPRITE);
  cgameMedia.sprites.splash0203 = cgi.LoadAtlasImage(spriteAtlas, "sprites/splash_02/splash_02_03", IMG_SPRITE);
  cgameMedia.sprites.impactSpark01Dot = cgi.LoadAtlasImage(spriteAtlas, "sprites/impact_spark_01/impact_spark_01_dot", IMG_SPRITE);
  cgameMedia.sprites.puffCloud = cgi.LoadAtlasImage(spriteAtlas, "sprites/puff_cloud", IMG_SPRITE);
  cgameMedia.sprites.waterCircle = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_circle", IMG_SPRITE);
  cgameMedia.sprites.waterRing = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_ring", IMG_SPRITE);
  cgameMedia.sprites.waterRing2 = cgi.LoadAtlasImage(spriteAtlas, "sprites/water/splash_01_ring2", IMG_SPRITE);
  cgameMedia.sprites.abstract01 = cgi.LoadAtlasImage(spriteAtlas, "sprites/abstract/abstract_01", IMG_SPRITE);
  cgameMedia.sprites.nodeWait = cgi.LoadAtlasImage(spriteAtlas, "pics/emoji/teamkill", IMG_SPRITE);
  cgameMedia.sprites.nodeSlow = cgi.LoadAtlasImage(spriteAtlas, "pics/emoji/crush", IMG_SPRITE);

  cgameMedia.sprites.blasterRing = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/blast_01/blast_01_ring", "_%02" PRIu32, 1, 7);
  cgameMedia.sprites.explosion = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/explosion_01/explosion_01", "_%02" PRIu32, 1, 36);
  cgameMedia.sprites.explosionRing02 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/explosion_ring_02/explosion_ring_02", "_%02" PRIu32, 1, 7);
  cgameMedia.sprites.rocketFlame = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/flame_03/flame_03", "_%02" PRIu32, 1, 29);
  cgameMedia.sprites.blasterFlame = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/flame_mono_01/flame_mono_01", "_%02" PRIu32, 1, 21);
  cgameMedia.sprites.smoke04 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/smoke_04/smoke_04", "_%02" PRIu32, 1, 90);
  cgameMedia.sprites.smoke05 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/smoke_05/smoke_05", "_%02" PRIu32, 1, 99);
  cgameMedia.sprites.bfgExplosion2 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/bfg_explosion_02/bfg_explosion_02", "_%02" PRIu32, 1, 23);
  cgameMedia.sprites.bfgExplosion3 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/bfg_explosion_03/bfg_explosion_03", "_%02" PRIu32, 1, 21);
  cgameMedia.sprites.poof01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/poof_01/poof_01", "_%02" PRIu32, 1, 34);
  cgameMedia.sprites.poof02 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/poof_02/poof_02", "_%02" PRIu32, 1, 17);
  cgameMedia.sprites.blood01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/blood_01/blood_01", "_%02" PRIu32, 1, 10);
  cgameMedia.sprites.electro01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/electro_01/electro_01", "_%02" PRIu32, 1, 5);
  cgameMedia.sprites.fireball01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/fireball_01/fireball_01", "_%02" PRIu32, 0, 63);
  cgameMedia.sprites.impactSpark01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/impact_spark_01/impact_spark_01", "_%02" PRIu32, 0, 4);
  cgameMedia.sprites.hyperball01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/hyperball_01/hyperball_01", "_%02" PRIu32, 1, 32);
  cgameMedia.sprites.fizz01 = Cg_LoadAnimatedSprite(spriteAtlas, "sprites/fizz_01/fizz_01", "_%02" PRIu32, 1, 24);

  cgi.LoadingProgress(-1, "compiling sprite atlas");

  cgi.CompileAtlas(spriteAtlas);

  cgi.LoadingProgress(-1, "decals");

  decalAtlas = cgi.LoadAtlas("cg_decal_atlas");

  for (size_t i = 0; i < lengthof(cgameMedia.decals.bullet); i++) {
    q_snprintf(name, sizeof(name), "decals/bullet_%zd", i);
    cgameMedia.decals.bullet[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameMedia.decals.blood); i++) {
    q_snprintf(name, sizeof(name), "decals/blood_%zd", i);
    cgameMedia.decals.blood[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameMedia.decals.burn); i++) {
    q_snprintf(name, sizeof(name), "decals/burn_%zd", i);
    cgameMedia.decals.burn[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
  }

  for (size_t i = 0; i < lengthof(cgameMedia.decals.slug); i++) {
      q_snprintf(name, sizeof(name), "decals/slug_%zd", i);
      cgameMedia.decals.slug[i] = cgi.LoadAtlasImage(decalAtlas, name, IMG_SPRITE);
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
