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

#include "cm_local.h"
#include <gl/GL.h>

/**
 * @brief stores the state of loaded materials. Materials contain a reference count,
 * which allow them to be shared between a collision model and a renderer view
 * without conflicting.
 */
typedef struct {

	/**
	 * @brief Hash table of loaded materials.
	 */
	GHashTable *materials;
} cm_materials_t;

static cm_materials_t cm_materials;

/**
 * @brief Free material callback
 */
static void Cm_Material_Free(gpointer data) {

	cm_material_t *material = (cm_material_t *) data;

	for (cm_stage_t *stage = material->stages; stage; stage = stage->next) {

		if (stage->material) {

			Cm_UnrefMaterial(stage->material);
		}
	}

	Mem_Free(material);
}

/**
 * @brief Load the common materials subsystem.
 */
void Cm_InitMaterials(void) {

	cm_materials.materials = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Cm_Material_Free);
}

/**
 * @brief Shutdown the common materials subsystem. This frees all of the materials,
 * so this should only be done on a complete system shutdown/reboot.
 */
void Cm_ShutdownMaterials(void) {

	g_hash_table_destroy(cm_materials.materials);
	cm_materials.materials = NULL;
}

/**
 * @brief Returns a reference to an already-loaded cm_material_t.
 */
cm_material_t *Cm_FindMaterial(const char *diffuse) {

	return (cm_material_t *) g_hash_table_lookup(cm_materials.materials, diffuse);
}

/**
 * @brief Decrease the reference count of the material by 1. If it reaches
 * zero, the material is freed and "mat" will no longer be valid.
 */
_Bool Cm_UnrefMaterial(cm_material_t *mat) {

	if (!mat->ref_count) {
		Com_Warn("Material ref count is bad: %s\n", mat->diffuse);
		return false;
	}

	mat->ref_count--;

	if (!mat->ref_count) {
		Com_Debug(DEBUG_COLLISION, "Unref material %s\n", mat->diffuse);
		g_hash_table_remove(cm_materials.materials, mat->key);
		return true;
	}

	Com_Debug(DEBUG_COLLISION, "Retain material %s\n", mat->diffuse);
	return false;
}

/**
 * @brief Increase the reference count of the material by 1. This is only needed
 * if you're transferring a pointer to a material somewhere else and the original
 * might get freed.
 */
void Cm_RefMaterial(cm_material_t *mat) {

	Com_Debug(DEBUG_COLLISION, "Ref material %s\n", mat->diffuse);
	mat->ref_count++;
}

/**
 * @brief
 */
static inline GLenum Cm_BlendConstByName(const char *c) {

	if (!g_strcmp0(c, "GL_ONE")) {
		return GL_ONE;
	}
	if (!g_strcmp0(c, "GL_ZERO")) {
		return GL_ZERO;
	}
	if (!g_strcmp0(c, "GL_SRC_ALPHA")) {
		return GL_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_ALPHA")) {
		return GL_ONE_MINUS_SRC_ALPHA;
	}
	if (!g_strcmp0(c, "GL_SRC_COLOR")) {
		return GL_SRC_COLOR;
	}
	if (!g_strcmp0(c, "GL_DST_COLOR")) {
		return GL_DST_COLOR;
	}
	if (!g_strcmp0(c, "GL_ONE_MINUS_SRC_COLOR")) {
		return GL_ONE_MINUS_SRC_COLOR;
	}

	// ...
	Com_Warn("Failed to resolve: %s\n", c);
	return GL_INVALID_ENUM;
}

/**
 * @brief
 */
static int32_t Cm_LoadStageFrames(cm_stage_t *s) {
	char name[MAX_QPATH];
	int32_t i, j;

	if (!*s->image) {
		Com_Warn("Texture not defined in anim stage\n");
		return -1;
	}

	g_strlcpy(name, s->image, sizeof(name));
	const size_t len = strlen(name);

	if ((i = (int32_t) strtol(&name[len - 1], NULL, 0)) < 0) {
		Com_Warn("Texture name does not end in numeric: %s\n", name);
		return -1;
	}

	// the first image was already loaded by the stage parse, so just copy
	// the pointer into the array

	s->anim.frames = Mem_LinkMalloc(s->anim.num_frames * sizeof(*s->anim.frames), s);
	s->anim.frames[0] = s->image;

	// now load the rest
	name[len - 1] = '\0';
	for (j = 1, i = i + 1; j < s->anim.num_frames; j++, i++) {
		char frame[MAX_QPATH];

		g_snprintf(frame, sizeof(frame), "%s%d", name, i);
		s->anim.frames[j] = Mem_LinkMalloc(strlen(frame) + 1, s);
		g_strlcpy(s->anim.frames[j], frame, strlen(frame));

		// IT_DIFFUSE
		/*if (s->anim.frames[j]->type == IT_NULL) {
			Com_Warn("Failed to resolve frame: %d: %s\n", j, frame);
			return -1;
		}*/
	}

	return 0;
}

/**
 * @brief
 */
static int32_t Cm_ParseStage(cm_stage_t *s, const char **buffer) {
	int32_t i;

	while (true) {

		const char *c = ParseToken(buffer);

		if (*c == '\0') {
			break;
		}

		if (!g_strcmp0(c, "texture") || !g_strcmp0(c, "diffuse")) {

			c = ParseToken(buffer);
			if (*c == '#') {
				g_strlcpy(s->image, ++c, sizeof(s->image));
			} else {
				g_strlcpy(s->image, va("textures/%s", c), sizeof(s->image));
			}
			
			// IT_DIFFUSE
			/*if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve texture: %s\n", c);
				return -1;
			}*/

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(c, "envmap")) {

			c = ParseToken(buffer);
			i = (int32_t) strtol(c, NULL, 0);

			if (*c == '#') {
				g_strlcpy(s->image, ++c, sizeof(s->image));
			} else if (*c == '0' || i > 0) {
				g_strlcpy(s->image, va("envmaps/envmap_%d", i), sizeof(s->image));
			} else {
				g_strlcpy(s->image, va("envmaps/%s", c), sizeof(s->image));
			}

			// IT_ENVMAP
			/*if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve envmap: %s\n", c);
				return -1;
			}*/

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (!g_strcmp0(c, "blend")) {

			c = ParseToken(buffer);
			s->blend.src = Cm_BlendConstByName(c);

			if (s->blend.src == GL_INVALID_ENUM) {
				Com_Warn("Failed to resolve blend src: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->blend.dest = Cm_BlendConstByName(c);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Com_Warn("Failed to resolve blend dest: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if (!g_strcmp0(c, "color")) {

			for (i = 0; i < 3; i++) {

				c = ParseToken(buffer);
				s->color[i] = strtod(c, NULL);

				if (s->color[i] < 0.0 || s->color[i] > 1.0) {
					Com_Warn("Failed to resolve color: %s\n", c);
					return -1;
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!g_strcmp0(c, "pulse")) {

			c = ParseToken(buffer);
			s->pulse.hz = strtod(c, NULL);

			if (s->pulse.hz == 0.0) {
				Com_Warn("Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_PULSE;
			continue;
		}

		if (!g_strcmp0(c, "stretch")) {

			c = ParseToken(buffer);
			s->stretch.amp = strtod(c, NULL);

			if (s->stretch.amp == 0.0) {
				Com_Warn("Failed to resolve amplitude: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->stretch.hz = strtod(c, NULL);

			if (s->stretch.hz == 0.0) {
				Com_Warn("Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_STRETCH;
			continue;
		}

		if (!g_strcmp0(c, "rotate")) {

			c = ParseToken(buffer);
			s->rotate.hz = strtod(c, NULL);

			if (s->rotate.hz == 0.0) {
				Com_Warn("Failed to resolve rotate: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ROTATE;
			continue;
		}

		if (!g_strcmp0(c, "scroll.s")) {

			c = ParseToken(buffer);
			s->scroll.s = strtod(c, NULL);

			if (s->scroll.s == 0.0) {
				Com_Warn("Failed to resolve scroll.s: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCROLL_S;
			continue;
		}

		if (!g_strcmp0(c, "scroll.t")) {

			c = ParseToken(buffer);
			s->scroll.t = strtod(c, NULL);

			if (s->scroll.t == 0.0) {
				Com_Warn("Failed to resolve scroll.t: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCROLL_T;
			continue;
		}

		if (!g_strcmp0(c, "scale.s")) {

			c = ParseToken(buffer);
			s->scale.s = strtod(c, NULL);

			if (s->scale.s == 0.0) {
				Com_Warn("Failed to resolve scale.s: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCALE_S;
			continue;
		}

		if (!g_strcmp0(c, "scale.t")) {

			c = ParseToken(buffer);
			s->scale.t = strtod(c, NULL);

			if (s->scale.t == 0.0) {
				Com_Warn("Failed to resolve scale.t: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_SCALE_T;
			continue;
		}

		if (!g_strcmp0(c, "terrain")) {

			c = ParseToken(buffer);
			s->terrain.floor = strtod(c, NULL);

			c = ParseToken(buffer);
			s->terrain.ceil = strtod(c, NULL);

			if (s->terrain.ceil <= s->terrain.floor) {
				Com_Warn("Invalid terrain ceiling and floor values for %s\n",
				         (*s->image ? s->image : "NULL"));
				return -1;
			}

			s->terrain.height = s->terrain.ceil - s->terrain.floor;
			s->flags |= STAGE_TERRAIN;
			continue;
		}

		if (!g_strcmp0(c, "dirtmap")) {

			c = ParseToken(buffer);
			s->dirt.intensity = strtod(c, NULL);

			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Com_Warn("Invalid dirtmap intensity for %s\n",
				         (*s->image ? s->image : "NULL"));
				return -1;
			}

			s->flags |= STAGE_DIRTMAP;
			continue;
		}

		if (!g_strcmp0(c, "anim")) {

			c = ParseToken(buffer);
			s->anim.num_frames = strtoul(c, NULL, 0);

			if (s->anim.num_frames < 1) {
				Com_Warn("Invalid number of anim frames for %s\n",
				         (*s->image ? s->image : "NULL"));
				return -1;
			}

			c = ParseToken(buffer);
			s->anim.fps = strtod(c, NULL);

			if (s->anim.fps < 0.0) {
				Com_Warn("Invalid anim fps for %s\n",
				         (*s->image ? s->image : "NULL"));
				return -1;
			}

			// the frame images are loaded once the stage is parsed completely

			s->flags |= STAGE_ANIM;
			continue;
		}

		if (!g_strcmp0(c, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (!g_strcmp0(c, "flare")) {

			c = ParseToken(buffer);
			i = (int32_t) strtol(c, NULL, 0);

			if (*c == '#') {
				g_strlcpy(s->image, ++c, sizeof(s->image));
			} else if (*c == '0' || i > 0) {
				g_strlcpy(s->image, va("flares/flare_%d", i), sizeof(s->image));
			} else {
				g_strlcpy(s->image, va("flares/%s", c), sizeof(s->image));
			}

			// IT_FLARE
			/*if (s->image->type == IT_NULL) {
				Com_Warn("Failed to resolve flare: %s\n", c);
				return -1;
			}*/

			s->flags |= STAGE_FLARE;
			continue;
		}

		if (*c == '}') {

			// a texture, or envmap means render it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP)) {
				s->flags |= STAGE_DIFFUSE;

				// a terrain blend or dirtmap means light it
				if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
					s->material = Cm_LoadMaterial(s->image);
					s->flags |= STAGE_LIGHTING;
				}
			}

			Com_Debug(DEBUG_COLLISION, 
					  "Parsed stage\n"
			          "  flags: %d\n"
			          "  texture: %s\n"
			          "   -> material: %s\n"
			          "  blend: %d %d\n"
			          "  color: %3f %3f %3f\n"
			          "  pulse: %3f\n"
			          "  stretch: %3f %3f\n"
			          "  rotate: %3f\n"
			          "  scroll.s: %3f\n"
			          "  scroll.t: %3f\n"
			          "  scale.s: %3f\n"
			          "  scale.t: %3f\n"
			          "  terrain.floor: %5f\n"
			          "  terrain.ceil: %5f\n"
			          "  anim.num_frames: %d\n"
			          "  anim.fps: %3f\n", s->flags, (*s->image ? s->image : "NULL"),
			          (s->material ? s->material->diffuse : "NULL"), s->blend.src,
			          s->blend.dest, s->color[0], s->color[1], s->color[2], s->pulse.hz,
			          s->stretch.amp, s->stretch.hz, s->rotate.hz, s->scroll.s, s->scroll.t,
			          s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil, s->anim.num_frames,
			          s->anim.fps);

			return 0;
		}
	}

	Com_Warn("Malformed stage\n");
	return -1;
}

/**
 * @brief Loads the r_material_t with the specified diffuse texture. This
 * increases ref count, so be sure to unref when you're done with the pointer.
 */
cm_material_t *Cm_LoadMaterial_(const char *where, const char *diffuse) {
	cm_material_t *mat;
	char name[MAX_QPATH], base[MAX_QPATH], key[MAX_QPATH];

	if (strstr(diffuse, "mossyrockies")) {
		Com_Debug(DEBUG_BREAKPOINT, "\n");
	}

	if (!diffuse || !diffuse[0]) {
		Com_Error(ERR_DROP, "NULL diffuse name\n");
	}

	StripExtension(diffuse, name);

	g_strlcpy(base, name, sizeof(base));

	if (g_str_has_suffix(base, "_d")) {
		base[strlen(base) - 2] = '\0';
	}

	g_snprintf(key, sizeof(key), "%s_mat", base);

	if (!(mat = Cm_FindMaterial(key))) {

		mat = (cm_material_t *) Mem_Malloc(sizeof(cm_material_t));

		mat->where = where;
		g_strlcpy(mat->diffuse, name, sizeof(mat->diffuse));
		g_strlcpy(mat->base, base, sizeof(mat->base));
		g_strlcpy(mat->key, key, sizeof(mat->key));

		mat->bump = DEFAULT_BUMP;
		mat->hardness = DEFAULT_HARDNESS;
		mat->parallax = DEFAULT_PARALLAX;
		mat->specular = DEFAULT_SPECULAR;

		g_hash_table_insert(cm_materials.materials, mat->key, mat);
	}

	Cm_RefMaterial(mat);

	return mat;
}

// REMOVEME: this is just to ensure that all materials are indeed gone after
// an r_restart.
static void Cm_Materials_ForEach(gpointer key, gpointer value, gpointer ud) {
	const char *keyName = (const char *) key;
	const cm_material_t *material = (const cm_material_t *) value;

	Com_Print("%s -> %s\n", key, material->where);
}

void Cm_DumpMaterialAllocations(void) {
	
	Com_Print("%u\n", g_hash_table_size(cm_materials.materials));

	g_hash_table_foreach(cm_materials.materials, Cm_Materials_ForEach, NULL);
}

/**
 * @brief Loads all of the materials for the specified .mat file. This must be
 * a full, absolute path to a .mat file WITH extension. The resulting materials all have
 * their reference counts increased, so be sure to free this bunch afterwards! A
 * GArray containing the list of materials loaded by this function is returned.
 */
GArray *Cm_LoadMaterials(const char *path) {
	void *buf;

	if (Fs_Load(path, &buf) == -1) {
		Com_Debug(DEBUG_COLLISION, "Couldn't load %s\n", path);
		return NULL;
	}

	const char *buffer = (char *) buf;
	GArray *materials = g_array_new(false, true, sizeof(cm_material_t *));

	_Bool in_material = false;
	cm_material_t *m = NULL;

	while (true) {

		const char *c = ParseToken(&buffer);

		if (*c == '\0') {
			break;
		}

		if (*c == '{' && !in_material) {
			in_material = true;
			continue;
		}

		if (!g_strcmp0(c, "material")) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				m = Cm_LoadMaterial(++c);
			} else {
				m = Cm_LoadMaterial(va("textures/%s", c));
			}

			g_array_append_val(materials, m);

			/*if (m->diffuse->type == IT_NULL) {
				Com_Warn("Failed to resolve %s\n", c);
				m = NULL;
			}*/

			continue;
		}

		if (!m) {
			continue;
		}

		if (!g_strcmp0(c, "normalmap")/* && r_bumpmap->value*/) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				g_strlcpy(m->normalmap, ++c, sizeof(m->normalmap));
			} else {
				g_strlcpy(m->normalmap, va("textures/%s", c), sizeof(m->normalmap));
			}

			// IT_NORMALMAP
			/*if (m->normalmap->type == IT_NULL) {
				Com_Warn("Failed to resolve normalmap: %s\n", c);
				m->normalmap = NULL;
			}*/
		}

		if (!g_strcmp0(c, "specularmap")/* && r_bumpmap->value*/) {
			c = ParseToken(&buffer);
			if (*c == '#') {
				g_strlcpy(m->specularmap, ++c, sizeof(m->specularmap));
			} else {
				g_strlcpy(m->specularmap, va("textures/%s", c), sizeof(m->specularmap));
			}

			// IT_SPECULARMAP
			/*if (m->specularmap->type == IT_NULL) {
				Com_Warn("Failed to resolve specularmap: %s\n", c);
				m->specularmap = NULL;
			}*/
		}

		if (!g_strcmp0(c, "bump")) {

			m->bump = strtod(ParseToken(&buffer), NULL);

			if (m->bump < 0.0) {
				Com_Warn("Invalid bump value for %s\n", m->diffuse);
				m->bump = DEFAULT_BUMP;
			}
		}

		if (!g_strcmp0(c, "parallax")) {

			m->parallax = strtod(ParseToken(&buffer), NULL);

			if (m->parallax < 0.0) {
				Com_Warn("Invalid parallax value for %s\n", m->diffuse);
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if (!g_strcmp0(c, "hardness")) {

			m->hardness = strtod(ParseToken(&buffer), NULL);

			if (m->hardness < 0.0) {
				Com_Warn("Invalid hardness value for %s\n", m->diffuse);
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(c, "specular")) {
			m->specular = strtod(ParseToken(&buffer), NULL);

			if (m->specular < 0.0) {
				Com_Warn("Invalid specular value for %s\n", m->diffuse);
				m->specular = DEFAULT_SPECULAR;
			}
		}

		if (*c == '{' && in_material) {

			cm_stage_t *s = (cm_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (Cm_ParseStage(s, &buffer) == -1) {
				Mem_Free(s);
				continue;
			}

			// load animation frame images
			if (s->flags & STAGE_ANIM) {
				if (Cm_LoadStageFrames(s) == -1) {
					Mem_Free(s);
					continue;
				}
			}

			// append the stage to the chain
			if (!m->stages) {
				m->stages = s;
			} else {
				cm_stage_t *ss = m->stages;
				while (ss->next) {
					ss = ss->next;
				}
				ss->next = s;
			}

			m->flags |= s->flags;
			m->num_stages++;
			continue;
		}

		if (*c == '}' && in_material) {
			Com_Debug(DEBUG_COLLISION, "Parsed material %s with %d stages\n", m->diffuse, m->num_stages);
			in_material = false;
			m = NULL;
		}
	}

	Fs_Free(buf);

	if (!materials->len) {
		g_array_free(materials, true);
		return NULL;
	}

	return materials;
}