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

#include "r_local.h"

/**
 * @brief Register event listener for materials.
 */
static void R_RegisterMaterial(r_media_t *self) {
	r_material_t *material = (r_material_t *) self;

	for (r_stage_t *stage = material->stages; stage; stage = stage->next) {
		if (stage->media) {
			R_RegisterDependency(self, stage->media);
		}
	}
}

/**
 * @brief Free event listener for materials.
 */
static void R_FreeMaterial(r_media_t *self) {

	Cm_FreeMaterial(((r_material_t *) self)->cm);
}

/**
 * @return The number of frames resolved, or -1 on error.
 */
static r_animation_t *R_LoadStageAnimation(r_stage_t *stage, cm_asset_context_t context) {

	const r_image_t *images[stage->cm->animation.num_frames];
	const r_image_t **out = images;

	for (int32_t i = 0; i < stage->cm->animation.num_frames; i++, out++) {

		cm_asset_t *frame = &stage->cm->animation.frames[i];
		if (*frame->path) {
			*out = R_LoadImage(frame->path, IT_MATERIAL);
		} else {
			*out = R_LoadImage("textures/common/notex", IT_MATERIAL);
			Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->cm->asset.name);
		}
	}

	return R_CreateAnimation(va("%s_animation", stage->cm->asset.name), stage->cm->animation.num_frames, images);
}

/**
 * @brief
 */
static void R_AppendStage(r_material_t *m, r_stage_t *s) {

	if (m->stages == NULL) {
		m->stages = s;
	} else {
		r_stage_t *stages = m->stages;
		while (stages->next) {
			stages = stages->next;
		}
		stages->next = s;
	}
}

/**
 * @brief Normalizes material names to their context, returning the unique media key for subsequent
 * material lookups.
 */
static void R_MaterialKey(const char *name, char *key, size_t len, cm_asset_context_t context) {

	*key = '\0';

	if (*name) {
		if (*name == '#') {
			name++;
		} else {
			switch (context) {
				case ASSET_CONTEXT_NONE:
					break;
				case ASSET_CONTEXT_TEXTURES:
					if (!g_str_has_prefix(name, "textures/")) {
						g_strlcat(key, "textures/", len);
					}
					break;
				case ASSET_CONTEXT_MODELS:
					if (!g_str_has_prefix(name, "models/")) {
						g_strlcat(key, "models/", len);
					}
					break;
				case ASSET_CONTEXT_PLAYERS:
					if (!g_str_has_prefix(name, "players/")) {
						g_strlcat(key, "players/", len);
					}
					break;
				case ASSET_CONTEXT_ENVMAPS:
					if (!g_str_has_prefix(name, "envmaps/")) {
						g_strlcat(key, "envmaps/", len);
					}
					break;
				case ASSET_CONTEXT_FLARES:
					if (!g_str_has_prefix(name, "flares/")) {
						g_strlcat(key, "flares/", len);
					}
					break;
			}
		}

		g_strlcat(key, name, len);
		g_strlcat(key, "_mat", len);
	}
}

/**
 * @brief Loads the material asset, ensuring it is the specified dimensions for texture layering.
 */
static SDL_Surface *R_LoadMaterialSurface(int32_t w, int32_t h, const char *path) {

	SDL_Surface *surface = Img_LoadSurface(path);
	if (surface) {
		if (w || h) {
			if (surface->w != w || surface->h != h) {
				Com_Warn("Material asset %s is not %dx%d, resizing..\n", path, w, h);

				SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
				SDL_BlitScaled(surface, NULL, scaled, NULL);

				SDL_FreeSurface(surface);
				surface = scaled;
			}
		}
	}

	return surface;
}

/**
 * @brief
 */
static SDL_Surface *R_CreateMaterialSurface(int32_t w, int32_t h, color32_t color) {

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);

	SDL_memset4(surface->pixels, color.rgba, w * h);

	return surface;
}

/**
 * @brief Merges the average of src's channels into the alpha channel of dest.
 */
static void R_MergeMaterialSurfaces(SDL_Surface *dest, const SDL_Surface *src) {

	assert(src->w == dest->w);
	assert(src->h == dest->h);

	const byte *in = src->pixels;
	byte *out = dest->pixels;

	const int32_t pixels = dest->w * dest->h;
	for (int32_t i = 0; i < pixels; i++, in += 4, out += 4) {
		out[3] = (in[0] + in[1] + in[2]) / 3.0;
	}
}

/**
 * @brief Resolves all asset references in the specified render material's stages
 */
static void R_ResolveMaterialStages(r_material_t *material, cm_asset_context_t context) {
	int32_t num_stages = 0;

	const cm_material_t *cm = material->cm;
	for (const cm_stage_t *cs = cm->stages; cs; cs = cs->next, num_stages++) {

		r_stage_t *stage = (r_stage_t *) Mem_LinkMalloc(sizeof(r_stage_t), material);
		stage->cm = cs;

		if (*stage->cm->asset.path) {
			if (stage->cm->flags & STAGE_ANIMATION) {
				stage->media = (r_media_t *) R_LoadStageAnimation(stage, context);
			} else if (stage->cm->flags & STAGE_MATERIAL) {
				stage->media = (r_media_t *) R_LoadMaterial(stage->cm->asset.name, context);
			} else {
				stage->media = (r_media_t *) R_LoadImage(stage->cm->asset.path, IT_MATERIAL);
			}

			assert(stage->media);

			R_RegisterDependency((r_media_t *) material, stage->media);
		}

		R_AppendStage(material, stage);
	}

	Com_Debug(DEBUG_RENDERER, "Resolved material %s with %d stages\n", material->cm->name, num_stages);
}

/**
 * @brief Resolves all asset references in the specified collision material, yielding a usable
 * renderer material.
 */
static r_material_t *R_ResolveMaterial(cm_material_t *cm, cm_asset_context_t context) {
	char key[MAX_QPATH];

	R_MaterialKey(cm->name, key, sizeof(key), context);

	r_material_t *material = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t), MEDIA_MATERIAL);
	material->cm = cm;

	material->media.Register = R_RegisterMaterial;
	material->media.Free = R_FreeMaterial;

	R_RegisterMedia((r_media_t *) material);

	material->texture = (r_image_t *) R_AllocMedia(va("%s_texture", material->cm->basename), sizeof(r_image_t), MEDIA_IMAGE);
	material->texture->type = IT_MATERIAL;

	R_RegisterDependency((r_media_t *) material, (r_media_t *) material->texture);

	Cm_ResolveMaterial(cm, context);
	
	SDL_Surface *diffusemap = NULL;
	if (*cm->diffusemap.path) {
		if ((diffusemap = Img_LoadSurface(cm->diffusemap.path))) {
			Com_Debug(DEBUG_RENDERER, "Loaded diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
		} else {
			Com_Warn("Failed to load diffusemap %s for %s\n", cm->diffusemap.path, cm->basename);
			diffusemap = Img_LoadSurface("textures/common/notex");
		}
	} else {
		diffusemap = Img_LoadSurface("textures/common/notex");
	}

	const int32_t w = material->texture->width = diffusemap->w;
	const int32_t h = material->texture->height = diffusemap->h;

	const size_t layer_size = w * h * 4;

	switch (context) {
		case ASSET_CONTEXT_TEXTURES:
		case ASSET_CONTEXT_MODELS:
		case ASSET_CONTEXT_PLAYERS: {

			SDL_Surface *normalmap = NULL;
			if (*cm->normalmap.path) {
				normalmap = R_LoadMaterialSurface(w, h, cm->normalmap.path);
				if (normalmap == NULL) {
					Com_Warn("Failed to load normalmap %s for %s\n", cm->normalmap.path, cm->basename);
					normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 127));
				}
			} else {
				normalmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 255, 127));
			}

			if (*cm->heightmap.path) {
				SDL_Surface *heightmap = R_LoadMaterialSurface(w, h, cm->heightmap.path);
				if (heightmap) {
					R_MergeMaterialSurfaces(normalmap, heightmap);
					SDL_FreeSurface(heightmap);
				} else {
					Com_Warn("Failed to load heightmap %s for %s\n", cm->heightmap.path, cm->basename);
				}
			}

			SDL_Surface *glossmap = NULL;
			if (*cm->glossmap.path) {
				glossmap = R_LoadMaterialSurface(w, h, cm->glossmap.path);
				if (glossmap == NULL) {
					Com_Warn("Failed to load glossmap %s for %s\n", cm->glossmap.path, cm->basename);
					glossmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 127, 127));
				}
			} else {
				glossmap = R_CreateMaterialSurface(w, h, Color32(127, 127, 127, 127));
			}

			if (*cm->specularmap.path) {
				SDL_Surface *specularmap = R_LoadMaterialSurface(w, h, cm->specularmap.path);
				if (specularmap) {
					R_MergeMaterialSurfaces(glossmap, specularmap);
					SDL_FreeSurface(specularmap);
				} else {
					Com_Warn("Failed to load specularmap %s for %s\n", cm->specularmap.path, cm->basename);
				}
			}
			
			SDL_Surface *tintmap = NULL;
			if (*cm->tintmap.path) {
				tintmap = R_LoadMaterialSurface(w, h, cm->tintmap.path);
				if (tintmap == NULL) {
					Com_Warn("Failed to load tintmap %s for %s\n", cm->tintmap.path, cm->basename);
					tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
				}
			} else {
				tintmap = R_CreateMaterialSurface(diffusemap->w, diffusemap->h, Color32(0, 0, 0, 0));
			}

			material->texture->depth = 4;

			byte *data = malloc(layer_size * material->texture->depth);

			memcpy(data + 0 * layer_size, diffusemap->pixels, layer_size);
			memcpy(data + 1 * layer_size, normalmap->pixels, layer_size);
			memcpy(data + 2 * layer_size, glossmap->pixels, layer_size);
			memcpy(data + 3 * layer_size, tintmap->pixels, layer_size);

			R_UploadImage(material->texture, GL_RGBA, data);

			free(data);

			SDL_FreeSurface(normalmap);
			SDL_FreeSurface(glossmap);
		}
			break;

		default:
			R_UploadImage(material->texture, GL_RGBA, diffusemap->pixels);
			break;
	}

	SDL_FreeSurface(diffusemap);

	R_ResolveMaterialStages(material, context);

	return material;
}

/**
 * @brief Finds an existing r_material_t from the specified texture, and registers it again if it exists.
 */
r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context) {
	char key[MAX_QPATH];
	char basename[MAX_QPATH];
	
	StripExtension(name, basename);
	R_MaterialKey(basename, key, sizeof(key), context);

	return (r_material_t *) R_FindMedia(key);
}

/**
 * @brief Loads the r_material_t from the specified texture.
 */
r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context) {

	r_material_t *material = R_FindMaterial(name, context);
	if (material == NULL) {
		material = R_ResolveMaterial(Cm_AllocMaterial(name), context);
	}

	return material;
}

/**
 * @brief Loads all materials defined in the given file.
 */
ssize_t R_LoadMaterials(const char *path, cm_asset_context_t context, GList **materials) {
	GList *source = NULL;
	const ssize_t count = Cm_LoadMaterials(path, &source);

	if (count > 0) {

		for (GList *list = source; list; list = list->next) {
			cm_material_t *cm = (cm_material_t *) list->data;

			r_material_t *material = R_FindMaterial(cm->basename, context);
			if (material == NULL) {
				material = R_ResolveMaterial(cm, context);
			}

			*materials = g_list_prepend(*materials, material);
		}
	}

	g_list_free(source);
	return count;
}

/**
 * @brief Loads all r_material_t for the specified BSP model.
 */
static void R_LoadBspMaterials(r_model_t *mod, GList **materials) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	R_LoadMaterials(path, ASSET_CONTEXT_TEXTURES, materials);

	const bsp_texinfo_t *in = mod->bsp->cm->file.texinfo;
	for (int32_t i = 0; i < mod->bsp->cm->file.num_texinfo; i++, in++) {

		r_material_t *material = R_LoadMaterial(in->texture, ASSET_CONTEXT_TEXTURES);

		if (g_list_find(*materials, material) == NULL) {
			*materials = g_list_prepend(*materials, material);
		}
	}
}

/**
 * @brief Loads all r_material_t for the specified mesh model.
 * @remarks Player models may optionally define materials, but are not required to.
 * @remarks Other mesh models must resolve at least one material. If no materials file is found,
 * we attempt to load ${model_dir}/skin.tga as the default material.
 */
static void R_LoadMeshMaterials(r_model_t *mod, GList **materials) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	if (g_str_has_prefix(mod->media.name, "players/")) {
		R_LoadMaterials(path, ASSET_CONTEXT_PLAYERS, materials);
	} else {
		if (R_LoadMaterials(path, ASSET_CONTEXT_MODELS, materials) < 1) {

			Dirname(mod->media.name, path);
			g_strlcat(path, "skin", sizeof(path));

			*materials = g_list_prepend(*materials, R_LoadMaterial(path, ASSET_CONTEXT_MODELS));
		}

		assert(materials);
	}
}

/**
 * @brief Loads all r_material_t for the specified model, populating `mod->materials`.
 */
void R_LoadModelMaterials(r_model_t *mod) {
	GList *materials = NULL;

	switch (mod->type) {
		case MOD_BSP:
			R_LoadBspMaterials(mod, &materials);
			break;
		case MOD_MESH:
			R_LoadMeshMaterials(mod, &materials);
			break;
		default:
			Com_Debug(DEBUG_RENDERER, "Unsupported model: %s\n", mod->media.name);
			break;
	}

	mod->num_materials = g_list_length(materials);
	mod->materials = Mem_LinkMalloc(sizeof(r_material_t *) * mod->num_materials, mod);

	r_material_t **out = mod->materials;
	for (const GList *list = materials; list; list = list->next, out++) {
		*out = (r_material_t *) R_RegisterDependency((r_media_t *) mod, (r_media_t *) list->data);
	}

	Com_Debug(DEBUG_RENDERER, "Loaded %" PRIuPTR " materials for %s\n", mod->num_materials, mod->media.name);

	g_list_free(materials);
}

/**
 * @brief Writes all r_material_t for the specified BSP model to disk.
 */
static ssize_t R_SaveBspMaterials(const r_model_t *mod) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	GList *materials = NULL;
	for (size_t i = 0; i < mod->num_materials; i++) {
		materials = g_list_prepend(materials, mod->materials[i]->cm);
	}

	const ssize_t count = Cm_WriteMaterials(path, materials);

	g_list_free(materials);
	return count;
}

/**
 * @brief
 */
void R_SaveMaterials_f(void) {

	if (!r_world_model) {
		Com_Print("No map loaded\n");
		return;
	}

	R_SaveBspMaterials(r_world_model);
}
