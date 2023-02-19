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
				case ASSET_CONTEXT_SPRITES:
					if (!g_str_has_prefix(name, "sprites/")) {
						g_strlcat(key, "sprites/", len);
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
 * @brief
 */
static SDL_Surface *R_CreateSpecularmap(SDL_Surface *diffusemap) {

	const color32_t *in = diffusemap->pixels;

	SDL_Surface *specularmap = SDL_CreateRGBSurfaceWithFormat(0, diffusemap->w, diffusemap->h, 32, SDL_PIXELFORMAT_RGBA32);
	color32_t *out = specularmap->pixels;

	for (int32_t i = 0; i < diffusemap->w; i++) {
		for (int32_t j = 0; j < diffusemap->h; j++, in++, out++) {
			out->r = out->g = out->b = (byte) (in->r + in->g + in->b) / 3.f;
			out->a = 255;
		}
	}

	return specularmap;
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

	r_material_t *material = (r_material_t *) R_AllocMedia(key, sizeof(r_material_t), R_MEDIA_MATERIAL);
	material->cm = cm;

	material->media.Register = R_RegisterMaterial;
	material->media.Free = R_FreeMaterial;

	R_RegisterMedia((r_media_t *) material);

	material->texture = (r_image_t *) R_AllocMedia(va("%s_texture", material->cm->basename), sizeof(r_image_t), R_MEDIA_IMAGE);
	material->texture->type = IT_MATERIAL;
	material->texture->target = GL_TEXTURE_2D;
	material->texture->internal_format = GL_RGBA8;
	material->texture->format = GL_RGBA;
	material->texture->pixel_type = GL_UNSIGNED_BYTE;

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

	if (r_texture_downsample->integer > 1) {
		SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(
				 0,
				 diffusemap->w / r_texture_downsample->integer,
				 diffusemap->h / r_texture_downsample->integer,
				 32,
				 SDL_PIXELFORMAT_RGBA32);
		SDL_BlitScaled(diffusemap, NULL, scaled, NULL);

		SDL_FreeSurface(diffusemap);
		diffusemap = scaled;
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

			SDL_Surface *specularmap = NULL;
			if (*cm->specularmap.path) {
				specularmap = R_LoadMaterialSurface(w, h, cm->specularmap.path);
				if (specularmap == NULL) {
					Com_Warn("Failed to load specularmap %s for %s\n", cm->specularmap.path, cm->basename);
					specularmap = R_CreateSpecularmap(diffusemap);
				}
			} else {
				specularmap = R_CreateSpecularmap(diffusemap);
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
			material->texture->target = GL_TEXTURE_2D_ARRAY;

			byte *data = malloc(layer_size * material->texture->depth);

			memcpy(data + 0 * layer_size, diffusemap->pixels, layer_size);
			memcpy(data + 1 * layer_size, normalmap->pixels, layer_size);
			memcpy(data + 2 * layer_size, specularmap->pixels, layer_size);
			memcpy(data + 3 * layer_size, tintmap->pixels, layer_size);

			R_UploadImage(material->texture, data);

			free(data);

			SDL_FreeSurface(normalmap);
			SDL_FreeSurface(specularmap);
			SDL_FreeSurface(tintmap);
		}
			break;

		default:
			R_UploadImage(material->texture, diffusemap->pixels);
			break;
	}

	material->color = Img_Color(diffusemap);
	
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

	return (r_material_t *) R_FindMedia(key, R_MEDIA_MATERIAL);
}

/**
 * @brief Loads the r_material_t with the specified asset name and context.
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
 * @brief Writes all r_material_t for the specified model to disk.
 */
static ssize_t R_SaveMaterials(const r_model_t *mod) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	GList *materials = NULL;

	switch (mod->type) {
		case MOD_BSP: {
			r_material_t **mat = mod->bsp->materials;
			for (int32_t i = 0; i < mod->bsp->num_materials; i++, mat++) {
				cm_material_t *cm = (*mat)->cm;
				materials = g_list_prepend(materials, cm);
			}
		}
			break;
		case MOD_MESH: {
			const r_mesh_face_t *face = mod->mesh->faces;
			for (int32_t i = 0; i < mod->mesh->num_faces; i++, face++) {
				cm_material_t *cm = face->material->cm;
				if (face->material && g_list_find(materials, cm) == NULL) {
					materials = g_list_prepend(materials, cm);
				}
			}
		}
			break;
		default:
			Com_Warn("Unsupported model: %s\n", mod->media.name);
			break;
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

	const r_model_t *model = r_world_model;

	if (Cmd_Argc() > 1) {
		model = (r_model_t *) R_FindMedia(Cmd_Argv(1), R_MEDIA_MODEL);
		if (model == NULL) {
			Com_Warn("Failed to save materials for %s\n", Cmd_Argv(1));
		}
	}

	assert(model);
	R_SaveMaterials(model);
}
