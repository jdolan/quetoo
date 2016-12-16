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
static inline const char *Cm_BlendNameByConst(const GLenum c) {

	switch (c)
	{
	case GL_ONE:
		return "GL_ONE";
	case GL_ZERO:
		return "GL_ZERO";
	case GL_SRC_ALPHA:
		return "GL_SRC_ALPHA";
	case GL_ONE_MINUS_SRC_ALPHA:
		return "GL_ONE_MINUS_SRC_ALPHA";
	case GL_SRC_COLOR:
		return "GL_SRC_COLOR";
	case GL_DST_COLOR:
		return "GL_DST_COLOR";
	case GL_ONE_MINUS_SRC_COLOR:
		return "GL_ONE_MINUS_SRC_COLOR";
	}

	// ...
	Com_Warn("Failed to resolve: %u\n", c);
	return "GL_INVALID_ENUM";
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
			g_strlcpy(s->image, c, sizeof(s->image));

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(c, "envmap")) {

			c = ParseToken(buffer);
			g_strlcpy(s->image, c, sizeof(s->image));
			s->image_index = (int32_t) strtol(c, NULL, 0);

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
			g_strlcpy(s->image, c, sizeof(s->image));
			s->image_index = (int32_t) strtol(c, NULL, 0);

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
	const cm_material_t *material = (const cm_material_t *) value;

	Com_Print("%s -> %s (%u)\n", key, material->where, material->ref_count);
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

			// backup the old full name
			char full_name[MAX_QPATH];
			g_strlcpy(full_name, c, sizeof(full_name));

			if (*c == '#') {
				m = Cm_LoadMaterial(++c);
			} else {
				m = Cm_LoadMaterial(va("textures/%s", c));
			}
			
			g_strlcpy(m->full_name, full_name, sizeof(m->full_name));
			g_strlcpy(m->material_file, path, sizeof(m->material_file));

			g_array_append_val(materials, m);
			continue;
		}

		if (!m) {
			continue;
		}

		if (!g_strcmp0(c, "normalmap")) {
			c = ParseToken(&buffer);
			g_strlcpy(m->normalmap, c, sizeof(m->normalmap));
		}

		if (!g_strcmp0(c, "specularmap")) {
			c = ParseToken(&buffer);
			g_strlcpy(m->specularmap, c, sizeof(m->specularmap));
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

/**
 * @brief Callback to close our file handles.
 */
static void Cm_WriteMaterials_Destroy(gpointer value) {

	Fs_Close((file_t *) value);
}

/**
 * @brief Serialize the given stage.
 */
static void Cm_WriteStage(const cm_material_t *material, const cm_stage_t *stage, file_t *file) {
	Fs_Print(file, "\t{\n");

	if (stage->flags & STAGE_TEXTURE) {
		Fs_Print(file, "\t\ttexture %s\n", stage->image);
	} else if (stage->flags & STAGE_ENVMAP) {
		Fs_Print(file, "\t\tenvmap %s\n", stage->image);
	} else if (stage->flags & STAGE_FLARE) {
		Fs_Print(file, "\t\tflare %s\n", stage->image);
	} else {
		Com_Warn("Material %s (from %s) has a stage with no texture\n", material->material_file, material->full_name);
	}

	if (stage->flags & STAGE_BLEND) {
		Fs_Print(file, "\t\tblend %s %s\n", Cm_BlendNameByConst(stage->blend.src), Cm_BlendNameByConst(stage->blend.dest));
	}

	if (stage->flags & STAGE_COLOR) {
		Fs_Print(file, "\t\tcolor %g %g %g\n", stage->color[0], stage->color[1], stage->color[2]);
	}

	if (stage->flags & STAGE_PULSE) {
		Fs_Print(file, "\t\tpulse %g\n", stage->pulse.hz);
	}

	if (stage->flags & STAGE_STRETCH) {
		Fs_Print(file, "\t\tstretch %g %g\n", stage->stretch.amp, stage->stretch.hz);
	}

	if (stage->flags & STAGE_ROTATE) {
		Fs_Print(file, "\t\trotate %g\n", stage->rotate.hz);
	}

	if (stage->flags & STAGE_SCROLL_S) {
		Fs_Print(file, "\t\tscroll.s %g\n", stage->scroll.s);
	}

	if (stage->flags & STAGE_SCROLL_T) {
		Fs_Print(file, "\t\tscroll.t %g\n", stage->scroll.t);
	}

	if (stage->flags & STAGE_SCALE_S) {
		Fs_Print(file, "\t\tscale.s %g\n", stage->scale.s);
	}

	if (stage->flags & STAGE_SCALE_T) {
		Fs_Print(file, "\t\tscale.t %g\n", stage->scale.t);
	}

	if (stage->flags & STAGE_TERRAIN) {
		Fs_Print(file, "\t\tterrain %g %g\n", stage->terrain.floor, stage->terrain.ceil);
	}

	if (stage->flags & STAGE_DIRTMAP) {
		Fs_Print(file, "\t\tdirtmap %g\n", stage->dirt.intensity);
	}

	if (stage->flags & STAGE_ANIM) {
		Fs_Print(file, "\t\tanim %u %g\n", stage->anim.num_frames, stage->anim.fps);
	}

	if (stage->flags & STAGE_LIGHTMAP) {
		Fs_Print(file, "\t\tlightmap\n");
	}

	Fs_Print(file, "\t}\n");
}

/**
 * @brief Serialize the given material.
 */
static void Cm_WriteMaterial(const cm_material_t *material, file_t *file) {
	Fs_Print(file, "{\n");

	// write the innards
	Fs_Print(file, "\tmaterial %s\n", material->full_name);
	
	if (*material->normalmap) {
		Fs_Print(file, "\tnormalmap %s\n", material->normalmap);
	}
	if (*material->specularmap) {
		Fs_Print(file, "\tspecularmap %s\n", material->specularmap);
	}
	
	if (material->bump != DEFAULT_BUMP) {
		Fs_Print(file, "\tbump %g\n", material->bump);
	}
	if (material->parallax != DEFAULT_BUMP) {
		Fs_Print(file, "\tparallax %g\n", material->parallax);
	}
	if (material->hardness != DEFAULT_BUMP) {
		Fs_Print(file, "\thardness %g\n", material->hardness);
	}
	if (material->specular != DEFAULT_BUMP) {
		Fs_Print(file, "\tspecular %g\n", material->specular);
	}

	// write stages
	for (cm_stage_t *stage = material->stages; stage; stage = stage->next) {
		Cm_WriteStage(material, stage, file);
	}

	Fs_Print(file, "}\n");
}

/**
 * @brief Serialize all of the .mat files that we have loaded in-memory back to the disk.
 */
void Cm_WriteMaterials(void) {
	GHashTable *files_opened = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Cm_WriteMaterials_Destroy);
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, cm_materials.materials);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		cm_material_t *material = (cm_material_t *) value;

		// don't write out materials with no mat file
		if (!*material->material_file) {
			continue;
		}

		file_t *file = g_hash_table_lookup(files_opened, material->material_file);

		if (!file) {
			file = Fs_OpenWrite(material->material_file);
			g_hash_table_insert(files_opened, material->material_file, file);
		}

		Cm_WriteMaterial(material, file);
	}

	// finish writing
	g_hash_table_destroy(files_opened);
}