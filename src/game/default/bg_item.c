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

#include "g_types.h"

#include "bg_item.h"

/**
 * @brief The complete list of item definitions, shared between game and cgame.
 */
const g_item_def_t bg_item_defs[] = {

  { /* ITEM_NONE */ },

  {
    .classname = "weapon_blaster",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/blaster/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_blaster",
    .name = "Blaster",
    .quantity = 0,
    .type = ITEM_WEAPON,
    .tag = WEAPON_BLASTER,
    .effect_color = { { 0.731f, 0.379f, 0.025f, 1.f } },
    .flags = WF_PROJECTILE,
    .priority = 0.10,
    .precaches = "weapons/blaster/fire.wav"
  },

  {
    .classname = "weapon_shotgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/shotgun/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_shotgun",
    .name = "Shotgun",
    .quantity = 1,
    .ammo = AMMO_SHELLS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_SHOTGUN,
    .effect_color = { { 0.731f, 0.379f, 0.025f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE | WF_MED_RANGE,
    .priority = 0.15,
    .precaches = "weapons/shotgun/fire.wav"
  },

  {
    .classname = "weapon_supershotgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/supershotgun/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_supershotgun",
    .name = "Super Shotgun",
    .quantity = 2,
    .ammo = AMMO_SHELLS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_SUPER_SHOTGUN,
    .effect_color = { { 0.731f, 0.379f, 0.025f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE,
    .priority = 0.25,
    .precaches = "weapons/supershotgun/fire.wav"
  },

  {
    .classname = "weapon_machinegun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/machinegun/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_machinegun",
    .name = "Machinegun",
    .quantity = 1,
    .ammo = AMMO_BULLETS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_MACHINEGUN,
    .effect_color = { { 0.739f, 0.556f, 0.032f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE | WF_MED_RANGE,
    .priority = 0.30,
    .precaches = "weapons/machinegun/fire_1.wav weapons/machinegun/fire_2.wav "
    "weapons/machinegun/fire_3.wav"
  },

  {
    .classname = "weapon_handgrenades",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/projectiles/grenade/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_handgrenades",
    .name = "Hand Grenades",
    .quantity = 1,
    .ammo = AMMO_GRENADES,
    .type = ITEM_WEAPON,
    .tag = WEAPON_HAND_GRENADE,
    .effect_color = { { 0.050f, 0.821f, 0.033f, 1.f } },
    .flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_TIMED | WF_MED_RANGE,
    .priority = 0.30,
    .precaches = "weapons/handgrenades/hg_throw.wav weapons/handgrenades/hg_clang.ogg "
    "weapons/handgrenades/hg_tick.ogg"
  },

  {
    .classname = "weapon_grenadelauncher",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/grenadelauncher/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_grenadelauncher",
    .name = "Grenade Launcher",
    .quantity = 1,
    .ammo = AMMO_GRENADES,
    .type = ITEM_WEAPON,
    .tag = WEAPON_GRENADE_LAUNCHER,
    .effect_color = { { 0.050f, 0.821f, 0.033f, 1.f } },
    .flags = WF_PROJECTILE | WF_EXPLOSIVE,
    .priority = 0.40,
    .precaches = "models/projectiles/grenade/tris.md3 weapons/grenadelauncher/fire.wav"
  },

  {
    .classname = "weapon_rocketlauncher",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/rocketlauncher/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_rocketlauncher",
    .name = "Rocket Launcher",
    .quantity = 1,
    .ammo = AMMO_ROCKETS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_ROCKET_LAUNCHER,
    .effect_color = { { 0.666f, 0.027f, 0.030f, 1.f } },
    .flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_MED_RANGE | WF_LONG_RANGE,
    .priority = 0.50,
    .precaches = "models/projectiles/rocket/tris.obj projectiles/rocket/fly.wav "
    "weapons/rocketlauncher/fire.wav"
  },

  {
    .classname = "weapon_hyperblaster",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/hyperblaster/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_hyperblaster",
    .name = "Hyperblaster",
    .quantity = 1,
    .ammo = AMMO_CELLS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_HYPERBLASTER,
    .effect_color = { { 0.046f, 0.669f, 0.683f, 1.f } },
    .flags = WF_PROJECTILE | WF_MED_RANGE,
    .priority = 0.50,
    .precaches = "weapons/hyperblaster/fire.wav weapons/hyperblaster/hit.wav"
  },

  {
    .classname = "weapon_lightning",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/lightning/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_lightning",
    .name = "Lightning Gun",
    .quantity = 1,
    .ammo = AMMO_BOLTS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_LIGHTNING,
    .effect_color = { { 0.624f, 0.663f, 0.751f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE,
    .priority = 0.50,
    .precaches = "weapons/lightning/fire.wav weapons/lightning/fly.wav "
    "weapons/lightning/discharge.wav"
  },

  {
    .classname = "weapon_railgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/railgun/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_railgun",
    .name = "Railgun",
    .quantity = 1,
    .ammo = AMMO_SLUGS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_RAILGUN,
    .effect_color = { { 0.040f, 0.082f, 0.587f, 1.f } },
    .flags = WF_HITSCAN | WF_LONG_RANGE,
    .priority = 0.60,
    .precaches = "weapons/railgun/fire.wav"
  },

  {
    .classname = "weapon_bfg",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/bfg/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_bfg",
    .name = "BFG10K",
    .quantity = 1,
    .ammo = AMMO_NUKES,
    .type = ITEM_WEAPON,
    .tag = WEAPON_BFG10K,
    .effect_color = { { 0.468f, 0.772f, 0.005f, 1.f } },
    .flags = WF_PROJECTILE | WF_MED_RANGE | WF_LONG_RANGE,
    .priority = 0.66,
    .precaches = "weapons/bfg/prime.wav weapons/bfg/hit.wav"
  },

  {
    .classname = "weapon_quake_shotgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_shotgun/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_shotgun",
    .name = "Shotgun",
    .quantity = 1,
    .ammo = AMMO_QUAKE_SHELLS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_SHOTGUN,
    .effect_color = { { 0.402f, 0.333f, 0.137f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE,
    .priority = 0.15,
    .precaches = "weapons/shotgun/fire.wav"
  },

  {
    .classname = "weapon_quake_supershotgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_supershotgun/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_supershotgun",
    .name = "Super Shotgun",
    .quantity = 2,
    .ammo = AMMO_QUAKE_SHELLS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_SUPER_SHOTGUN,
    .effect_color = { { 0.402f, 0.333f, 0.137f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE,
    .priority = 0.25,
    .precaches = "weapons/supershotgun/fire.wav"
  },

  {
    .classname = "weapon_quake_nailgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_nailgun/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_nailgun",
    .name = "Nailgun",
    .quantity = 1,
    .ammo = AMMO_QUAKE_NAILS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_NAILGUN,
    .effect_color = { { 0.652f, 0.664f, 0.604f, 1.f } },
    .flags = WF_HITSCAN | WF_MED_RANGE,
    .priority = 0.30,
    .precaches = "weapons/machinegun/fire_1.wav"
  },

  {
    .classname = "weapon_quake_supernailgun",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_supernailgun/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_supernailgun",
    .name = "Super Nailgun",
    .quantity = 2,
    .ammo = AMMO_QUAKE_NAILS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_SUPER_NAILGUN,
    .effect_color = { { 0.652f, 0.664f, 0.604f, 1.f } },
    .flags = WF_HITSCAN | WF_MED_RANGE,
    .priority = 0.40,
    .precaches = "weapons/machinegun/fire_1.wav"
  },

  {
    .classname = "weapon_quake_grenadelauncher",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_grenadelauncher/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_grenadelauncher",
    .name = "Grenade Launcher",
    .quantity = 1,
    .ammo = AMMO_QUAKE_ROCKETS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_GRENADE_LAUNCHER,
    .effect_color = { { 0.600f, 0.200f, 0.050f, 1.f } },
    .flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_MED_RANGE,
    .priority = 0.45,
    .precaches = "weapons/grenadelauncher/fire.wav"
  },

  {
    .classname = "weapon_quake_rocketlauncher",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_rocketlauncher/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_rocketlauncher",
    .name = "Rocket Launcher",
    .quantity = 1,
    .ammo = AMMO_QUAKE_ROCKETS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_ROCKET_LAUNCHER,
    .effect_color = { { 0.600f, 0.200f, 0.050f, 1.f } },
    .flags = WF_PROJECTILE | WF_EXPLOSIVE | WF_MED_RANGE | WF_LONG_RANGE,
    .priority = 0.60,
    .precaches = "weapons/rocketlauncher/fire.wav"
  },

  {
    .classname = "weapon_quake_thunderbolt",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/quake_thunderbolt/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/w_quake_thunderbolt",
    .name = "Thunderbolt",
    .quantity = 1,
    .ammo = AMMO_QUAKE_BOLTS,
    .type = ITEM_WEAPON,
    .tag = WEAPON_QUAKE_THUNDERBOLT,
    .effect_color = { { 0.434f, 0.341f, 0.065f, 1.f } },
    .flags = WF_HITSCAN | WF_SHORT_RANGE,
    .priority = 0.55,
    .precaches = "weapons/lightning/fire.wav weapons/lightning/fly.wav "
    "weapons/lightning/discharge.wav"
  },

  {
    .classname = "ammo_shells",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/shells/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_shells",
    .name = "Shells",
    .quantity = 10,
    .max = 80,
    .type = ITEM_AMMO,
    .tag = AMMO_SHELLS,
    .effect_color = { { 0.731f, 0.379f, 0.025f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_bullets",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/bullets/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_bullets",
    .name = "Bullets",
    .quantity = 50,
    .max = 200,
    .type = ITEM_AMMO,
    .tag = AMMO_BULLETS,
    .effect_color = { { 0.739f, 0.556f, 0.032f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_grenades",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/grenades/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_handgrenades",
    .name = "Grenades",
    .quantity = 10,
    .max = 50,
    .type = ITEM_AMMO,
    .tag = AMMO_GRENADES,
    .effect_color = { { 0.050f, 0.821f, 0.033f, 1.f } },
    .priority = 0.15,
    .precaches = "weapons/handgrenades/hg_throw.wav weapons/handgrenades/hg_clang.ogg "
    "weapons/handgrenades/hg_tick.ogg"
  },

  {
    .classname = "ammo_rockets",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/rockets/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_rockets",
    .name = "Rockets",
    .quantity = 10,
    .max = 50,
    .type = ITEM_AMMO,
    .tag = AMMO_ROCKETS,
    .effect_color = { { 0.666f, 0.027f, 0.030f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_cells",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/cells/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_cells",
    .name = "Cells",
    .quantity = 50,
    .max = 200,
    .type = ITEM_AMMO,
    .tag = AMMO_CELLS,
    .effect_color = { { 0.046f, 0.669f, 0.683f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_bolts",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/bolts/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_bolts",
    .name = "Bolts",
    .quantity = 25,
    .max = 150,
    .type = ITEM_AMMO,
    .tag = AMMO_BOLTS,
    .effect_color = { { 0.624f, 0.663f, 0.751f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_slugs",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/slugs/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_slugs",
    .name = "Slugs",
    .quantity = 10,
    .max = 50,
    .type = ITEM_AMMO,
    .tag = AMMO_SLUGS,
    .effect_color = { { 0.040f, 0.082f, 0.587f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_nukes",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/nukes/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/a_nukes",
    .name = "Nukes",
    .quantity = 2,
    .max = 10,
    .type = ITEM_AMMO,
    .tag = AMMO_NUKES,
    .effect_color = { { 0.468f, 0.772f, 0.005f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_quake_shells",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/quake_shells/tris.obj",
    .effects = 0,
    .icon = "pics/a_quake_shells",
    .name = "Shells",
    .quantity = 10,
    .max = 80,
    .type = ITEM_AMMO,
    .tag = AMMO_QUAKE_SHELLS,
    .effect_color = { { 0.402f, 0.333f, 0.137f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_quake_nails",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/quake_nails/tris.obj",
    .effects = 0,
    .icon = "pics/a_quake_nails",
    .name = "Nails",
    .quantity = 25,
    .max = 200,
    .type = ITEM_AMMO,
    .tag = AMMO_QUAKE_NAILS,
    .effect_color = { { 0.652f, 0.664f, 0.604f, 1.f } },
    .priority = 0.15,
    .precaches = ""
  },

  {
    .classname = "ammo_quake_rockets",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/quake_rockets/tris.obj",
    .effects = 0,
    .icon = "pics/a_quake_rockets",
    .name = "Rockets",
    .quantity = 5,
    .max = 100,
    .type = ITEM_AMMO,
    .tag = AMMO_QUAKE_ROCKETS,
    .effect_color = { { 0.600f, 0.200f, 0.050f, 1.f } },
    .priority = 0.20,
    .precaches = ""
  },

  {
    .classname = "ammo_quake_bolts",
    .pickup_sound = "ammo/common/pickup.wav",
    .model = "models/ammo/quake_bolts/tris.obj",
    .effects = 0,
    .icon = "pics/a_quake_bolts",
    .name = "Bolts",
    .quantity = 15,
    .max = 200,
    .type = ITEM_AMMO,
    .tag = AMMO_QUAKE_BOLTS,
    .effect_color = { { 0.434f, 0.341f, 0.065f, 1.f } },
    .priority = 0.20,
    .precaches = ""
  },

  {
    .classname = "item_armor_shard",
    .pickup_sound = "armor/shard/pickup.wav",
    .model = "models/armor/shard/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_armor_shard",
    .name = "Armor Shard",
    .quantity = 2,
    .type = ITEM_ARMOR,
    .tag = ARMOR_SHARD,
    .effect_color = { { 0.3f, 0.8f, 0.2f, 1.f } },
    .priority = 0.10,
    .precaches = ""
  },

  {
    .classname = "item_armor_jacket",
    .pickup_sound = "armor/jacket/pickup.wav",
    .model = "models/armor/jacket/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_armor_jacket",
    .name = "Jacket Armor",
    .quantity = 25,
    .max = 50,
    .type = ITEM_ARMOR,
    .tag = ARMOR_JACKET,
    .effect_color = { { 0.122f, 0.628f, 0.143f, 1.f } },
    .priority = 0.50,
    .precaches = ""
  },

  {
    .classname = "item_armor_combat",
    .pickup_sound = "armor/combat/pickup.wav",
    .model = "models/armor/combat/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_armor_combat",
    .name = "Combat Armor",
    .quantity = 50,
    .max = 100,
    .type = ITEM_ARMOR,
    .tag = ARMOR_COMBAT,
    .effect_color = { { 0.511f, 0.423f, 0.105f, 1.f } },
    .priority = 0.66,
    .precaches = ""
  },

  {
    .classname = "item_armor_body",
    .pickup_sound = "armor/body/pickup.wav",
    .model = "models/armor/body/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_armor_body",
    .name = "Body Armor",
    .quantity = 100,
    .max = 200,
    .type = ITEM_ARMOR,
    .tag = ARMOR_BODY,
    .effect_color = { { 0.436f, 0.145f, 0.130f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_quake_armor_jacket",
    .pickup_sound = "armor/jacket/pickup.wav",
    .model = "models/armor/quake_jacket/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/i_quake_armor_jacket",
    .name = "Green Armor",
    .quantity = 100,
    .max = 100,
    .type = ITEM_ARMOR,
    .tag = ARMOR_QUAKE_JACKET,
    .effect_color = { { 0.254f, 0.475f, 0.342f, 1.f } },
    .priority = 0.60,
    .precaches = ""
  },

  {
    .classname = "item_quake_armor_combat",
    .pickup_sound = "armor/combat/pickup.wav",
    .model = "models/armor/quake_combat/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/i_quake_armor_combat",
    .name = "Yellow Armor",
    .quantity = 150,
    .max = 150,
    .type = ITEM_ARMOR,
    .tag = ARMOR_QUAKE_COMBAT,
    .effect_color = { { 0.554f, 0.495f, 0.016f, 1.f } },
    .priority = 0.75,
    .precaches = ""
  },

  {
    .classname = "item_quake_armor_body",
    .pickup_sound = "armor/body/pickup.wav",
    .model = "models/armor/quake_body/tris.obj",
    .effects = EF_ROTATE,
    .icon = "pics/i_quake_armor_body",
    .name = "Red Armor",
    .quantity = 200,
    .max = 200,
    .type = ITEM_ARMOR,
    .tag = ARMOR_QUAKE_BODY,
    .effect_color = { { 0.793f, 0.024f, 0.071f, 1.f } },
    .priority = 0.90,
    .precaches = ""
  },

  {
    .classname = "item_health_small",
    .pickup_sound = "health/small/pickup.wav",
    .model = "models/health/small/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_health_small",
    .name = "Small Health",
    .quantity = 3,
    .type = ITEM_HEALTH,
    .tag = HEALTH_SMALL,
    .effect_color = { { 0.405f, 0.707f, 0.014f, 1.f } },
    .priority = 0.10,
    .precaches = ""
  },

  {
    .classname = "item_health",
    .pickup_sound = "health/medium/pickup.wav",
    .model = "models/health/medium/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_health_medium",
    .name = "Medium Health",
    .quantity = 15,
    .type = ITEM_HEALTH,
    .tag = HEALTH_MEDIUM,
    .effect_color = { { 0.866f, 0.594f, 0.023f, 1.f } },
    .priority = 0.25,
    .precaches = ""
  },

  {
    .classname = "item_health_large",
    .pickup_sound = "health/large/pickup.wav",
    .model = "models/health/large/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_health_large",
    .name = "Large Health",
    .quantity = 25,
    .type = ITEM_HEALTH,
    .tag = HEALTH_LARGE,
    .effect_color = { { 0.754f, 0.153f, 0.016f, 1.f } },
    .priority = 0.40,
    .precaches = ""
  },

  {
    .classname = "item_health_mega",
    .pickup_sound = "health/mega/pickup.wav",
    .model = "models/health/mega/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_health_mega",
    .name = "Mega Health",
    .quantity = 100,
    .type = ITEM_HEALTH,
    .tag = HEALTH_MEGA,
    .effect_color = { { 0.029f, 0.462f, 0.741f, 1.f } },
    .priority = 0.60,
    .precaches = ""
  },

  {
    .classname = "item_health_quake_medium",
    .pickup_sound = "health/medium/pickup.wav",
    .model = "models/health/quake_medium/tris.obj",
    .effects = 0,
    .icon = "pics/i_quake_health_medium",
    .name = "Medium Health",
    .quantity = 15,
    .type = ITEM_HEALTH,
    .tag = HEALTH_QUAKE_MEDIUM,
    .effect_color = { { 0.513f, 0.288f, 0.179f, 1.f } },
    .priority = 0.25,
    .precaches = ""
  },

  {
    .classname = "item_health_quake_large",
    .pickup_sound = "health/large/pickup.wav",
    .model = "models/health/quake_large/tris.obj",
    .effects = 0,
    .icon = "pics/i_quake_health_large",
    .name = "Large Health",
    .quantity = 25,
    .type = ITEM_HEALTH,
    .tag = HEALTH_QUAKE_LARGE,
    .effect_color = { { 0.566f, 0.251f, 0.143f, 1.f } },
    .priority = 0.40,
    .precaches = ""
  },

  {
    .classname = "item_health_quake_mega",
    .pickup_sound = "health/mega/pickup.wav",
    .model = "models/health/quake_mega/tris.obj",
    .effects = 0,
    .icon = "pics/i_quake_health_mega",
    .name = "Mega Health",
    .quantity = 100,
    .type = ITEM_HEALTH,
    .tag = HEALTH_QUAKE_MEGA,
    .effect_color = { { 0.620f, 0.167f, 0.107f, 1.f } },
    .priority = 0.60,
    .precaches = ""
  },

  {
    .classname = "item_quad",
    .pickup_sound = "powerups/quad/pickup.wav",
    .model = "models/powerups/quad/tris.obj",
    .effects = EF_ROTATE | EF_LIGHT | EF_LIGHT_PULSE,
    .icon = "pics/i_quad",
    .name = "Quad Damage",
    .quantity = 0,
    .type = ITEM_POWERUP,
    .tag = POWERUP_QUAD,
    .effect_color = { { 0.079f, 0.309f, 0.507f, 1.f } },
    .priority = 0.80,
    .precaches = "powerups/quad/attack.wav powerups/quad/expire.wav",
    .light_color = { { .2f, .4f, 1.f } },
    .light_radius = 160.f,
  },

  {
    .classname = "item_adrenaline",
    .pickup_sound = "powerups/adrenaline/pickup.wav",
    .model = "models/powerups/adrenaline/tris.obj",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/i_adrenaline",
    .name = "Adrenaline",
    .quantity = 0,
    .type = ITEM_POWERUP,
    .tag = POWERUP_ADRENALINE,
    .effect_color = { { 0.022f, 0.627f, 0.069f, 1.f } },
    .priority = 0.45,
    .precaches = ""
  },

  {
    .classname = "item_invulnerability",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/powerups/invulnerability/tris.obj",
    .effects = EF_ROTATE | EF_LIGHT | EF_LIGHT_PULSE,
    .icon = "pics/i_invulnerability",
    .name = "Invulnerability",
    .quantity = 0,
    .type = ITEM_POWERUP,
    .tag = POWERUP_INVULNERABILITY,
    .effect_color = { { 0.492f, 0.004f, 0.015f, 1.f } },
    .priority = 0.90,
    .precaches = "powerups/invulnerability/expire.wav",
    .light_color = { { 1.f, .2f, .2f } },
    .light_radius = 160.f
  },

  {
    .classname = "item_invisibility",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/powerups/invisibility/tris.obj",
    .effects = EF_ROTATE | EF_LIGHT | EF_LIGHT_PULSE,
    .icon = "pics/i_invisibility",
    .name = "Invisibility",
    .quantity = 0,
    .type = ITEM_POWERUP,
    .tag = POWERUP_INVISIBILITY,
    .effect_color = { { 0.844f, 0.569f, 0.210f, 1.f } },
    .priority = 0.80,
    .precaches = "powerups/invisibility/expire.wav",
    .light_color = { { 1.f, .9f, 0.f } },
    .light_radius = 120.f
  },

  {
    .classname = "item_tech_haste",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/techs/haste/tris.obj",
    .effects = EF_BOB | EF_ROTATE,
    .icon = "pics/i_haste",
    .name = "Haste",
    .quantity = 0,
    .type = ITEM_TECH,
    .tag = TECH_HASTE,
    .effect_color = { { 0.306f, 0.233f, 0.053f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_tech_regen",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/techs/regen/tris.obj",
    .effects = EF_BOB | EF_ROTATE,
    .icon = "pics/i_regen",
    .name = "Regeneration",
    .quantity = 0,
    .type = ITEM_TECH,
    .tag = TECH_REGEN,
    .effect_color = { { 0.151f, 0.293f, 0.090f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_tech_resist",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/techs/resist/tris.obj",
    .effects = EF_BOB | EF_ROTATE,
    .icon = "pics/i_resist",
    .name = "Resist",
    .quantity = 0,
    .type = ITEM_TECH,
    .tag = TECH_RESIST,
    .effect_color = { { 0.091f, 0.137f, 0.273f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_tech_strength",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/techs/strength/tris.obj",
    .effects = EF_BOB | EF_ROTATE,
    .icon = "pics/i_strength",
    .name = "Strength",
    .quantity = 0,
    .type = ITEM_TECH,
    .tag = TECH_STRENGTH,
    .effect_color = { { 0.261f, 0.044f, 0.047f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_tech_vampire",
    .pickup_sound = "powerups/common/pickup.wav",
    .model = "models/techs/vampire/tris.obj",
    .effects = EF_BOB | EF_ROTATE,
    .icon = "pics/i_vampire",
    .name = "Vampire",
    .quantity = 0,
    .type = ITEM_TECH,
    .tag = TECH_VAMPIRE,
    .effect_color = { { 0.200f, 0.087f, 0.252f, 1.f } },
    .priority = 0.80,
    .precaches = ""
  },

  {
    .classname = "item_flag_team1",
    .model = "models/ctf/flag/tris.obj",
    .effects = EF_BOB | EF_ROTATE | EF_TEAM_TINT,
    .icon = "pics/i_flag1",
    .name = "Enemy Flag",
    .quantity = 0,
    .type = ITEM_FLAG,
    .tag = FLAG_RED,
    .effect_color = { { 0.90f, 0.20f, 0.20f, 1.f } },
    .priority = 0.75,
    .precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
  },

  {
    .classname = "item_flag_team2",
    .model = "models/ctf/flag/tris.obj",
    .effects = EF_BOB | EF_ROTATE | EF_TEAM_TINT,
    .icon = "pics/i_flag2",
    .name = "Enemy Flag",
    .quantity = 0,
    .type = ITEM_FLAG,
    .tag = FLAG_BLUE,
    .effect_color = { { 0.25f, 0.40f, 1.00f, 1.f } },
    .priority = 0.75,
    .precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
  },

  {
    .classname = "item_flag_team3",
    .model = "models/ctf/flag/tris.obj",
    .effects = EF_BOB | EF_ROTATE | EF_TEAM_TINT,
    .icon = "pics/i_flag3",
    .name = "Enemy Flag",
    .quantity = 0,
    .type = ITEM_FLAG,
    .tag = FLAG_YELLOW,
    .effect_color = { { 1.00f, 0.90f, 0.20f, 1.f } },
    .priority = 0.75,
    .precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
  },

  {
    .classname = "item_flag_team4",
    .model = "models/ctf/flag/tris.obj",
    .effects = EF_BOB | EF_ROTATE | EF_TEAM_TINT,
    .icon = "pics/i_flag4",
    .name = "Enemy Flag",
    .quantity = 0,
    .type = ITEM_FLAG,
    .tag = FLAG_GREEN,
    .effect_color = { { 0.20f, 0.90f, 0.30f, 1.f } },
    .priority = 0.75,
    .precaches = "ctf/capture.wav ctf/steal.wav ctf/return.wav"
  },
};

size_t bg_num_items = sizeof(bg_item_defs) / sizeof(bg_item_defs[0]);
