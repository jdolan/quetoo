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
#include <GL/gl.h>

/**
 * @brief Free the memory for the specified material. This does not free the whole
 * list, so if this contains linked materials, be sure to rid them as well.
 */
void Cm_FreeMaterial(cm_material_t *material) {

	if (material->list) {
		g_queue_remove(material->list, material);

		if (g_queue_is_empty(material->list)) {
			g_queue_free(material->list);
		}
	}

	Mem_Free(material);
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

	switch (c) {
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
static int32_t Cm_ParseStage(cm_material_t *m, cm_stage_t *s, const char **buffer) {
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

		if (!g_strcmp0(c, "fog")) {
			s->flags |= STAGE_FOG;
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
					Mem_Link(s->material, m);
					s->flags |= STAGE_LIGHTING;
				}
			}

			// a blend dest other than GL_ONE should use fog by default
			if (s->blend.dest != GL_ONE) {
				s->flags |= STAGE_FOG;
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
			          (s->material ? s->material->stages->image : "NULL"), s->blend.src,
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
 * @brief Normalizes a material's input name and fills the buffer with the base name.
 */
void Cm_MaterialName(const char *in, char *out, size_t len) {

	if (out != in) {
		g_strlcpy(out, in, len);
	}

	if (g_str_has_suffix(out, "_d")) {
		out[strlen(out) - 2] = '\0';
	}
}

/**
 * @brief
 */
static void Cm_AttachStage(cm_material_t *m, cm_stage_t *s) {

	m->num_stages++;

	if (!m->stages) {
		m->stages = s;
		return;
	}

	cm_stage_t *ss = m->stages;
	while (ss->next) {
		ss = ss->next;
	}
	ss->next = s;
}

/**
 * @brief Loads the material and sets up the diffuse stage.
 */
cm_material_t *Cm_LoadMaterial(const char *diffuse) {

	if (!diffuse || !diffuse[0]) {
		Com_Error(ERROR_DROP, "NULL diffuse name\n");
	}

	cm_material_t *mat = Mem_TagMalloc(sizeof(cm_material_t), MEM_TAG_MATERIAL);
	cm_stage_t *stage = (cm_stage_t *) Mem_LinkMalloc(sizeof(*stage), mat);

	StripExtension(diffuse, stage->image);
	Cm_MaterialName(diffuse, mat->base, sizeof(mat->base));

	stage->diffuse.bump = DEFAULT_BUMP;
	stage->diffuse.hardness = DEFAULT_HARDNESS;
	stage->diffuse.parallax = DEFAULT_PARALLAX;
	stage->diffuse.specular = DEFAULT_SPECULAR;

	stage->flags |= STAGE_TEXTURE | STAGE_DIFFUSE;
	mat->flags |= stage->flags;

	Cm_AttachStage(mat, stage);

	return mat;
}

/**
 * @brief Loads all of the materials for the specified .mat file. This must be
 * a full, absolute path to a .mat file WITH extension. The resulting materials are all
 * linked together by the returned material pointer, so don't lose that one!
 */
cm_material_t *Cm_LoadMaterials(const char *path, size_t *count) {
	void *buf;

	if (Fs_Load(path, &buf) == -1) {
		Com_Debug(DEBUG_COLLISION, "Couldn't load %s\n", path);
		return NULL;
	}

	const char *buffer = (char *) buf;
	cm_material_t *m = NULL;
	_Bool in_material = false, parsing_material = false;

	if (count) {
		*count = 0;
	}

	GQueue *list = g_queue_new();

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

			cm_material_t *mat;

			if (*c == '#') {
				mat = Cm_LoadMaterial(c + 1);
			} else {
				mat = Cm_LoadMaterial(va("textures/%s", c));
			}

			g_strlcpy(mat->name, c, sizeof(mat->name));

			m = mat;

			parsing_material = true;
			continue;
		}

		if (!m && !parsing_material) {
			continue;
		}

		if (!g_strcmp0(c, "normalmap")) {
			c = ParseToken(&buffer);
			g_strlcpy(m->stages->diffuse.normalmap, c, sizeof(m->stages->diffuse.normalmap));
		}

		if (!g_strcmp0(c, "specularmap")) {
			c = ParseToken(&buffer);
			g_strlcpy(m->stages->diffuse.specularmap, c, sizeof(m->stages->diffuse.specularmap));
		}

		if (!g_strcmp0(c, "bump")) {

			m->stages->diffuse.bump = strtod(ParseToken(&buffer), NULL);

			if (m->stages->diffuse.bump < 0.0) {
				Com_Warn("Invalid bump value for %s\n", m->base);
				m->stages->diffuse.bump = DEFAULT_BUMP;
			}
		}

		if (!g_strcmp0(c, "parallax")) {

			m->stages->diffuse.parallax = strtod(ParseToken(&buffer), NULL);

			if (m->stages->diffuse.parallax < 0.0) {
				Com_Warn("Invalid parallax value for %s\n", m->base);
				m->stages->diffuse.parallax = DEFAULT_PARALLAX;
			}
		}

		if (!g_strcmp0(c, "hardness")) {

			m->stages->diffuse.hardness = strtod(ParseToken(&buffer), NULL);

			if (m->stages->diffuse.hardness < 0.0) {
				Com_Warn("Invalid hardness value for %s\n", m->base);
				m->stages->diffuse.hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(c, "specular")) {
			m->stages->diffuse.specular = strtod(ParseToken(&buffer), NULL);

			if (m->stages->diffuse.specular < 0.0) {
				Com_Warn("Invalid specular value for %s\n", m->base);
				m->stages->diffuse.specular = DEFAULT_SPECULAR;
			}
		}

		if (!g_strcmp0(c, "footsteps")) {
			const char *footsteps = ParseToken(&buffer);
			g_strlcpy(m->stages->diffuse.footsteps, footsteps, sizeof(m->stages->diffuse.footsteps));
		}

		if (!g_strcmp0(c, "diffuse")) {

			c = ParseToken(&buffer);

			if (*c == '{') {
				cm_stage_t *s = m->stages;

				if (Cm_ParseStage(m, s, &buffer) == -1) {
					Com_Debug(DEBUG_COLLISION, "Couldn't load a stage in %s\n", m->base);
					Mem_Free(s);
					continue;
				}

				m->flags |= s->flags;
				continue;
			}
		}

		if (*c == '{' && in_material) {

			cm_stage_t *s = (cm_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (Cm_ParseStage(m, s, &buffer) == -1) {
				Com_Debug(DEBUG_COLLISION, "Couldn't load a stage in %s\n", m->base);
				Mem_Free(s);
				continue;
			}

			// append the stage to the chain
			Cm_AttachStage(m, s);

			m->flags |= s->flags;
			continue;
		}

		if (*c == '}' && in_material) {
			m->list = list;
			g_queue_push_tail(list, m);

			Com_Debug(DEBUG_COLLISION, "Parsed material %s with %d stages\n", m->base, m->num_stages);
			in_material = false;
			parsing_material = false;

			if (count) {
				(*count)++;
			}
		}
	}

	Fs_Free(buf);
	return m;
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
		Com_Warn("Material %s has a stage with no texture?\n", material->name);
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
static void Cm_WriteMaterial_(const cm_material_t *material, file_t *file) {
	Fs_Print(file, "{\n");

	// write the innards
	Fs_Print(file, "\tmaterial %s\n", material->name);

	if (*material->stages->diffuse.normalmap) {
		Fs_Print(file, "\tnormalmap %s\n", material->stages->diffuse.normalmap);
	}
	if (*material->stages->diffuse.specularmap) {
		Fs_Print(file, "\tspecularmap %s\n", material->stages->diffuse.specularmap);
	}

	if (material->stages->diffuse.bump != DEFAULT_BUMP) {
		Fs_Print(file, "\tbump %g\n", material->stages->diffuse.bump);
	}
	if (material->stages->diffuse.parallax != DEFAULT_PARALLAX) {
		Fs_Print(file, "\tparallax %g\n", material->stages->diffuse.parallax);
	}
	if (material->stages->diffuse.hardness != DEFAULT_HARDNESS) {
		Fs_Print(file, "\thardness %g\n", material->stages->diffuse.hardness);
	}
	if (material->stages->diffuse.specular != DEFAULT_SPECULAR) {
		Fs_Print(file, "\tspecular %g\n", material->stages->diffuse.specular);
	}

	// if not empty/default, write footsteps
	if (*material->stages->diffuse.footsteps && g_strcmp0(material->stages->diffuse.footsteps, "default")) {
		Fs_Print(file, "\tfootsteps %s\n", material->stages->diffuse.footsteps);
	}

	// write stages
	for (cm_stage_t *stage = material->stages + 1; stage; stage = stage->next) {
		Cm_WriteStage(material, stage, file);
	}

	Fs_Print(file, "}\n");
}

/**
 * @brief Serialize the material(s) into the specified file.
 */
void Cm_WriteMaterial(const char *filename, const cm_material_t *material) {
	file_t *file = Fs_OpenWrite(filename);

	for (GList *list = material->list->head; list; list = list->next) {
		Cm_WriteMaterial_((const cm_material_t *) list->data, file);
	}

	Fs_Close(file);
}
