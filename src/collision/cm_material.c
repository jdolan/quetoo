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

#include <SDL_opengl.h>

#include "cm_local.h"

/**
 * @brief Free the specified material.
 */
void Cm_FreeMaterial(cm_material_t *material) {
	Mem_Free(material);
}

/**
 * @brief Free the specified list and all included materials.
 */
void Cm_FreeMaterials(GList *materials) {
	g_list_free_full(materials, (GDestroyNotify) Cm_FreeMaterial);
}

/**
 * @brief Just a tuple for string-int
 */
typedef struct {
	const char *keyword;
	union {
		const int32_t flag;
		const GLenum enumVal;
	};
} cm_dictionary_t;

/**
 * @brief Content flags
 */
static cm_dictionary_t cm_contents_dict[] = {
	{ .keyword = "solid", .flag = CONTENTS_SOLID },
	{ .keyword = "window", .flag = CONTENTS_WINDOW },
	{ .keyword = "lava", .flag = CONTENTS_LAVA },
	{ .keyword = "slime", .flag = CONTENTS_SLIME },
	{ .keyword = "water", .flag = CONTENTS_WATER },
	{ .keyword = "mist", .flag = CONTENTS_MIST },
	{ .keyword = "detail", .flag = CONTENTS_DETAIL },
	{ .keyword = "ladder", .flag = CONTENTS_LADDER }
};

/**
 * @brief
 */
static int32_t Cm_ParseContents(const char *c) {

	int32_t contents = 0;

	for (cm_dictionary_t *dict = cm_contents_dict; dict < cm_contents_dict + lengthof(cm_contents_dict); dict++) {
		if (strstr(c, dict->keyword)) {
			contents |= dict->flag;
		}
	}

	return contents;
}

/**
 * @brief
 */
static char *Cm_UnparseContents(int32_t contents) {
	static char s[MAX_STRING_CHARS];
	*s = '\0';

	for (cm_dictionary_t *dict = cm_contents_dict; dict < cm_contents_dict + lengthof(cm_contents_dict); dict++) {
		if (contents & dict->flag) {
			g_strlcat(s, va("%s ", dict->keyword), sizeof(s));
		}
	}

	return g_strchomp(s);
}

/**
 * @brief Surface flags
 */
static cm_dictionary_t cm_surfaceList[] = {
	{ .keyword = "light", .flag = SURF_LIGHT },
	{ .keyword = "slick", .flag = SURF_SLICK },
	{ .keyword = "sky", .flag = SURF_SKY },
	{ .keyword = "liquid", .flag = SURF_LIQUID },
	{ .keyword = "blend_33", .flag = SURF_BLEND_33 },
	{ .keyword = "blend_66", .flag = SURF_BLEND_66 },
	{ .keyword = "blend_100", .flag = SURF_BLEND_100 },
	{ .keyword = "no_draw", .flag = SURF_NO_DRAW },
	{ .keyword = "hint", .flag = SURF_HINT },
	{ .keyword = "skip", .flag = SURF_SKIP },
	{ .keyword = "alpha_test", .flag = SURF_ALPHA_TEST },
	{ .keyword = "phong", .flag = SURF_PHONG },
	{ .keyword = "material", .flag = SURF_MATERIAL },
	{ .keyword = "decal", .flag = SURF_DECAL },
	{ .keyword = "debug_luxel", .flag = SURF_DEBUG_LUXEL }
};

/**
 * @brief
 */
static int32_t Cm_ParseSurface(const char *c) {

	int32_t surface = 0;

	for (cm_dictionary_t *list = cm_surfaceList; list < cm_surfaceList + lengthof(cm_surfaceList); list++) {
		if (strstr(c, list->keyword)) {
			surface |= list->flag;
		}
	}

	return surface;
}

/**
 * @brief
 */
static char *Cm_UnparseSurface(int32_t surface) {
	static char s[MAX_STRING_CHARS];
	*s = '\0';

	for (cm_dictionary_t *list = cm_surfaceList; list < cm_surfaceList + lengthof(cm_surfaceList); list++) {
		if (surface & list->flag) {
			g_strlcat(s, va("%s ", list->keyword), sizeof(s));
		}
	}

	return g_strchomp(s);
}

/**
 * @brief Blend consts
 */
static cm_dictionary_t cm_blendConstList[] = {
	{ .keyword = "GL_ONE", .enumVal = GL_ONE },
	{ .keyword = "GL_ZERO", .enumVal = GL_ZERO },
	{ .keyword = "GL_SRC_ALPHA", .enumVal = GL_SRC_ALPHA },
	{ .keyword = "GL_ONE_MINUS_SRC_ALPHA", .enumVal = GL_ONE_MINUS_SRC_ALPHA },
	{ .keyword = "GL_SRC_COLOR", .enumVal = GL_SRC_COLOR },
	{ .keyword = "GL_DST_COLOR", .enumVal = GL_DST_COLOR },
	{ .keyword = "GL_ONE_MINUS_SRC_COLOR", .enumVal = GL_ONE_MINUS_SRC_COLOR }
};

/**
 * @brief
 */
static inline GLenum Cm_BlendConstByName(const char *c) {
	
	for (cm_dictionary_t *list = cm_blendConstList; list < cm_blendConstList + lengthof(cm_blendConstList); list++) {
		if (!g_strcmp0(c, list->keyword)) {
			return list->enumVal;
		}
	}

	Com_Warn("Failed to resolve: %s\n", c);
	return GL_INVALID_ENUM;
}

/**
 * @brief
 */
static inline const char *Cm_BlendNameByConst(const GLenum c) {
	
	for (cm_dictionary_t *list = cm_blendConstList; list < cm_blendConstList + lengthof(cm_blendConstList); list++) {
		if (c == list->enumVal) {
			return list->keyword;
		}
	}

	Com_Warn("Failed to resolve: %u\n", c);
	return "GL_INVALID_ENUM";
}

/**
 * @brief
 */
static void Cm_MaterialWarn(const char *path, const parser_t *parser, const char *message) {
	Com_Warn("%s: Syntax error (Ln %u Col %u)\n", path, parser->position.row + 1, parser->position.col);

	if (message) {
		Com_Warn("  %s\n", message);
	}
}

/**
 * @brief
 */
static _Bool Cm_ParseStage(cm_material_t *m, cm_stage_t *s, parser_t *parser, const char *path) {
	char token[MAX_TOKEN_CHARS];

	while (true) {

		if (!Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "texture")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->asset.name, sizeof(s->asset.name))) {
				Cm_MaterialWarn(path, parser, "Missing texture name");
				continue;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(token, "blend")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, parser, "Missing blend src");
				continue;
			}

			s->blend.src = Cm_BlendConstByName(token);

			if (s->blend.src == GL_INVALID_ENUM) {
				Cm_MaterialWarn(path, parser, "Invalid blend src");
				s->blend.src = 0;
			}

			if (!Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, parser, "Missing blend dest");
				continue;
			}

			s->blend.dest = Cm_BlendConstByName(token);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Cm_MaterialWarn(path, parser, "Invalid blend dest");
				s->blend.dest = 0;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if (!g_strcmp0(token, "color")) {

			const size_t count = Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, s->color.rgba, 4);
			if (count != 4) {
				if (count == 3) {
					s->color.a = 1.f;
				} else {
					Cm_MaterialWarn(path, parser, "Missing color values");
					continue;
				}
			}

			for (int32_t i = 0; i < 4; i++) {
				if (s->color.rgba[i] < 0.0f) {
					Cm_MaterialWarn(path, parser, "Invalid value for color");
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!g_strcmp0(token, "pulse")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->pulse.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for pulse");
				continue;
			}

			if (s->pulse.hz == 0.0f) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_PULSE;
			}

			continue;
		}

		if (!g_strcmp0(token, "stretch")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.amp, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for amplitude");
				continue;
			}

			if (s->stretch.amp == 0.0f) {
				Cm_MaterialWarn(path, parser, "Amplitude must not be zero");
			}

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for frequency");
				continue;
			}

			if (s->stretch.hz == 0.0f) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			}

			if (s->stretch.amp != 0.0f &&
				s->stretch.hz != 0.0f) {
				s->flags |= STAGE_STRETCH;
			}

			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->rotate.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for rotate");
				continue;
			}

			if (s->rotate.hz == 0.0f) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_ROTATE;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.s")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.s, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for scroll.s");
				continue;
			}

			if (s->scroll.s == 0.0f) {
				Cm_MaterialWarn(path, parser, "scroll.s must not be zero");
			} else {
				s->flags |= STAGE_SCROLL_S;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.t")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.t, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for scroll.t");
				continue;
			}

			if (s->scroll.t == 0.0) {
				Cm_MaterialWarn(path, parser, "scroll.t must not be zero");
			} else {
				s->flags |= STAGE_SCROLL_T;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale.s")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scale.s, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for scale.s");
				continue;
			}

			if (s->scale.s == 0.0) {
				Cm_MaterialWarn(path, parser, "scale.s must not be zero");
			} else {
				s->flags |= STAGE_SCALE_S;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale.t")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scale.t, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for scale.t");
				continue;
			}

			if (s->scale.t == 0.0) {
				Cm_MaterialWarn(path, parser, "scale.t must not be zero");
			} else {
				s->flags |= STAGE_SCALE_T;
			}

			continue;
		}

		if (!g_strcmp0(token, "terrain")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->terrain.floor, 1) != 1 ||
				Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->terrain.ceil, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Missing floor or ceiling for terrain");
				continue;
			}

			if (s->terrain.ceil <= s->terrain.floor) {
				Cm_MaterialWarn(path, parser, "Terrain ceiling must be > floor");
			} else {
				s->flags |= STAGE_TERRAIN;
			}

			continue;
		}

		if (!g_strcmp0(token, "dirtmap")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->dirtmap.intensity, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for dirtmap");
				continue;
			}

			if (s->dirtmap.intensity <= 0.0 || s->dirtmap.intensity > 1.0) {
				Cm_MaterialWarn(path, parser, "Dirtmap intensity must be between 0.0 and 1.0");
			} else {
				s->flags |= STAGE_DIRTMAP;
			}

			continue;
		}

		if (!g_strcmp0(token, "envmap")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->asset.name, sizeof(s->asset.name))) {
				Cm_MaterialWarn(path, parser, "Missing envmap asset or index");
				continue;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (!g_strcmp0(token, "warp")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->warp.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for warp hz");
				continue;
			}

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->warp.amplitude, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for warp amplitude");
				continue;
			}

			s->flags |= STAGE_WARP;
		}

		if (!g_strcmp0(token, "shell")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->shell.radius, 1) != 1) {
				Cm_MaterialWarn(path, parser, "No value provided for shell radius");
				continue;
			}

			s->flags |= STAGE_SHELL;
			continue;
		}

		if (!g_strcmp0(token, "anim")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_UINT16, &s->animation.num_frames, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need number of frames");
				continue;
			}

			if (s->animation.num_frames < 1) {
				Cm_MaterialWarn(path, parser, "Invalid number of frames");
			}

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->animation.fps, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need FPS value");
				continue;
			}

			if (s->animation.fps < 0.0) {
				Cm_MaterialWarn(path, parser, "Invalid FPS value, must be >= 0.0");
			}

			// the frame images are loaded once the stage is parsed completely
			if (s->animation.num_frames && s->animation.fps >= 0.0) {
				s->flags |= STAGE_ANIMATION;
			}

			continue;
		}

		if (!g_strcmp0(token, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (!g_strcmp0(token, "fog")) {
			s->flags |= STAGE_FOG;
			continue;
		}

		if (!g_strcmp0(token, "flare")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->asset.name, sizeof(s->asset.name))) {
				Cm_MaterialWarn(path, parser, "Missing flare asset or index");
				continue;
			}

			s->flags |= STAGE_FLARE;
			continue;
		}

		if (*token == '}') {

			// a warp stage with no texture will use the material texture
			if ((s->flags & STAGE_WARP) && !(s->flags & STAGE_TEXTURE)) {
				s->asset = m->diffusemap;
			}

			// a texture or envmap mean draw it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP | STAGE_WARP | STAGE_SHELL)) {
				s->flags |= STAGE_DRAW;

				// terrain and dirtmapping use lighting
				if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
					s->flags |= STAGE_MATERIAL;
				}
			}

			// ensure appropriate blend function defaults
			if (s->blend.src == 0) {
				s->blend.src = GL_SRC_ALPHA;
			}

			if (s->blend.dest == 0) {
				s->blend.dest = GL_ONE_MINUS_SRC_ALPHA;
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
			          "  color: %.1f %.1f %.1f %.1f\n"
			          "  pulse: %.1f\n"
			          "  stretch: %.1f %.1f\n"
			          "  rotate: %.1f\n"
			          "  scroll.s: %.1f\n"
			          "  scroll.t: %.1f\n"
			          "  scale.s: %.1f\n"
			          "  scale.t: %.1f\n"
			          "  terrain.floor: %.1f\n"
			          "  terrain.ceil: %.1f\n"
			          "  anim.num_frames: %d\n"
			          "  anim.fps: %.1f\n", s->flags, (*s->asset.name ? s->asset.name : "NULL"),
			          ((s->flags & STAGE_MATERIAL) ? "true" : "false"), s->blend.src,
			          s->blend.dest, s->color.r, s->color.g, s->color.b, s->color.a, s->pulse.hz,
			          s->stretch.amp, s->stretch.hz, s->rotate.hz, s->scroll.s, s->scroll.t,
			          s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil, s->animation.num_frames,
			          s->animation.fps);

			return true;
		}
	}

	Cm_MaterialWarn(path, parser, va("Malformed stage for material %s", m->basename));
	return false;
}

/**
 * @brief Normalizes a material's input name and fills the buffer with the base name.
 */
void Cm_MaterialBasename(const char *in, char *out, size_t len) {

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
static void Cm_AppendStage(cm_material_t *m, cm_stage_t *s) {

	if (m->stages == NULL) {
		m->stages = s;
	} else {
		cm_stage_t *ss = m->stages;
		while (ss->next) {
			ss = ss->next;
		}
		ss->next = s;
	}
}

/**
 * @brief
 */
static int32_t Cm_StageIndex(const cm_material_t *m, const cm_stage_t *stage) {
	int32_t i = 0;

	for (const cm_stage_t *s = m->stages; s; s = s->next, i++) {
		if (s == stage) {
			return i;
		}
	}

	return -1;
}

/**
 * @brief Allocates a material, setting up the diffuse stage.
 */
cm_material_t *Cm_AllocMaterial(const char *name) {

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL diffuse name\n");
	}

	cm_material_t *mat = Mem_TagMalloc(sizeof(cm_material_t), MEM_TAG_MATERIAL);

	StripExtension(name, mat->name);

	Cm_MaterialBasename(mat->name, mat->basename, sizeof(mat->basename));

	mat->roughness = DEFAULT_ROUGHNESS;
	mat->hardness = DEFAULT_HARDNESS;
	mat->specularity = DEFAULT_SPECULARITY;
	mat->bloom = DEFAULT_BLOOM;

	return mat;
}

/**
 * @brief Loads the materials defined in the specified file.
 * @param path The Quake path of the materials file.
 * @param materials The list to append to.
 * @return The number of materials parsed, or -1 on error.
 */
ssize_t Cm_LoadMaterials(const char *path, GList **materials) {
	void *buf;
	ssize_t count = 0;

	if (Fs_Load(path, &buf) == -1) {
		Com_Debug(DEBUG_COLLISION, "Couldn't load %s\n", path);
		return -1;
	}

	parser_t parser = Parse_Init((const char *) buf, PARSER_C_LINE_COMMENTS | PARSER_C_BLOCK_COMMENTS);

	cm_material_t *m = NULL;
	_Bool in_material = false;

	while (true) {
		char token[MAX_TOKEN_CHARS];

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (*token == '{' && !in_material) {
			in_material = true;
			continue;
		}

		if (!g_strcmp0(token, "material") || !g_strcmp0(token, "diffusemap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, MAX_QPATH)) {
				Cm_MaterialWarn(path, &parser, "Too many characters in path");
				break;
			}

			m = Cm_AllocMaterial(token);
			assert(m);

			for (const GList *list = *materials; list; list = list->next) {
				const cm_material_t *mat = list->data;
				if (!g_strcmp0(m->basename, mat->basename)) {
					Cm_MaterialWarn(path, &parser, "Ignoring redefined material");
					Cm_FreeMaterial(m);
					m = NULL;
					break;
				}
			}

			continue;
		}

		if (!m) {
			continue;
		}

		if (!g_strcmp0(token, "normalmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->normalmap.name, sizeof(m->normalmap.name))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!g_strcmp0(token, "specularmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->specularmap.name, sizeof(m->specularmap.name))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!g_strcmp0(token, "tintmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->tintmap.name, sizeof(m->tintmap.name))) {
				Cm_MaterialWarn(path, &parser, "Missing path or too many characters");
				continue;
			}
		}

		if (!strncmp(token, "tintmap.", strlen("tintmap."))) {
			static vec4_t unused_color;
			vec4_t *color = &unused_color;

			if (!g_strcmp0(token, "tintmap.tint_r_default")) {
				color = &m->tintmap_defaults[TINT_R];
			} else if (!g_strcmp0(token, "tintmap.tint_g_default")) {
				color = &m->tintmap_defaults[TINT_G];
			} else if (!g_strcmp0(token, "tintmap.tint_b_default")) {
				color = &m->tintmap_defaults[TINT_B];
			} else {
				Cm_MaterialWarn(path, &parser, va("Invalid token \"%s\"", token));
			}

			const size_t num_parsed = Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, color->xyzw, 4);
			if (num_parsed < 3 || num_parsed > 4) {
				Cm_MaterialWarn(path, &parser, "Invalid color (must be 3 or 4 components)");
			} else {
				if (num_parsed != 4) {
					color->w = 1.f;
				}

				for (size_t i = 0; i < num_parsed; i++) {
					if (color->xyzw[i] < 0.f || color->xyzw[i] > 1.f) {
						Cm_MaterialWarn(path, &parser, "Color number out of range (must be between 0.0 and 1.0)");
					}
				}
			}

			continue;
		}

		if (!g_strcmp0(token, "roughness")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->roughness, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No roughness specified");
			} else if (m->roughness < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid roughness value, must be >= 0.0");
				m->roughness = DEFAULT_ROUGHNESS;
			}
		}

		if (!g_strcmp0(token, "hardness")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->hardness, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No hardness specified");
			} else if (m->hardness < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid hardness value, must be >= 0.0");
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(token, "specularity")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->specularity, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No specularity specified");
			} else if (m->specularity < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid specularity value, must be >= 0.0");
				m->specularity = DEFAULT_SPECULARITY;
			}
		}

		if (!g_strcmp0(token, "bloom")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->bloom, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No bloom specified");
			} else if (m->bloom < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid bloom value, must be >= 0.0");
				m->bloom = DEFAULT_BLOOM;
			}
		}

		if (!g_strcmp0(token, "alpha_test")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->alpha_test, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No alpha test specified");
			} else if (m->alpha_test < 0.f || m->alpha_test > 1.f) {
				Cm_MaterialWarn(path, &parser, "Invalid alpha test value, must be > 0.0 and < 1.0");
				m->alpha_test = DEFAULT_ALPHA_TEST;
			}

			m->surface |= SURF_ALPHA_TEST;
		}

		if (!g_strcmp0(token, "contents")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No contents specified");
			} else {
				m->contents |= Cm_ParseContents(token);
			}
			continue;
		}

		if (!g_strcmp0(token, "surface")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No surface flags specified");
			} else {
				m->surface |= Cm_ParseSurface(token);
			}
			continue;
		}

		if (!g_strcmp0(token, "patch_size")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->patch_size, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No patch size specified");
			} else if (m->patch_size < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid patch size value, must be > 0.0");
				m->patch_size = 0.f;
			}
		}

		if (!g_strcmp0(token, "footsteps")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->footsteps.name, sizeof(m->footsteps.name))) {
				Cm_MaterialWarn(path, &parser, "Invalid footsteps value");
			}
		}

		if (!g_strcmp0(token, "light") || !g_strcmp0(token, "light.radius")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->light, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No light radius specified");
			} else if (m->light.radius < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid light radius, must be > 0.0");
				m->light.radius = 0.f;
			}

			m->surface |= SURF_LIGHT;
		}

		if (!g_strcmp0(token, "light.intensity")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->light.intensity, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No light intensity specified");
			} else if (m->light.intensity < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid light intensity, must be > 0.0");
				m->light.intensity = 0.f;
			}
		}

		if (!g_strcmp0(token, "light.cone")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->light.cone, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No light cone specified");
			} else if (m->light.cone < 0.f) {
				Cm_MaterialWarn(path, &parser, "Invalid light cone, must be > 0.0");
				m->light.cone = 0.f;
			}
		}

		if (*token == '{' && in_material) {

			cm_stage_t *s = (cm_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (!Cm_ParseStage(m, s, &parser, path)) {
				Com_Debug(DEBUG_COLLISION, "Couldn't load a stage in %s", m->name);
				Mem_Free(s);
				continue;
			}

			// append the stage to the chain
			Cm_AppendStage(m, s);

			m->stage_flags |= s->flags;
			continue;
		}

		if (*token == '}' && in_material) {
			*materials = g_list_prepend(*materials, m);
			in_material = false;
			count++;

			Com_Debug(DEBUG_COLLISION, "Parsed material %s\n", m->name);

			m = NULL;
		}
	}

	Fs_Free(buf);

	return count;
}

/**
 * @brief Resolves the path of the specified asset by name within the given context.
 */
static _Bool Cm_ResolveAsset(cm_asset_t *asset, cm_asset_context_t context) {
	const char *extensions[] = { "tga", "png", "jpg", "pcx", "wal" };
	char name[MAX_QPATH];

	if (asset->name[0] == '#') {
		g_strlcpy(name, asset->name + 1, sizeof(name));
	} else {
		g_strlcpy(name, asset->name, sizeof(name));

		switch (context) {
			case ASSET_CONTEXT_NONE:
				break;
			case ASSET_CONTEXT_TEXTURES:
				if (!g_str_has_prefix(asset->name, "textures/")) {
					g_snprintf(name, sizeof(name), "textures/%s", asset->name);
				}
				break;
			case ASSET_CONTEXT_MODELS:
				if (!g_str_has_prefix(asset->name, "models/")) {
					g_snprintf(name, sizeof(name), "models/%s", asset->name);
				}
				break;
			case ASSET_CONTEXT_PLAYERS:
				if (!g_str_has_prefix(asset->name, "players/")) {
					g_snprintf(name, sizeof(name), "players/%s", asset->name);
				}
				break;
			case ASSET_CONTEXT_SPRITES:
				if (!g_str_has_prefix(asset->name, "sprites/")) {
					g_snprintf(name, sizeof(name), "sprites/%s", asset->name);
				}
				break;
		}
	}

	for (size_t i = 0; i < lengthof(extensions); i++) {
		g_snprintf(asset->path, sizeof(asset->path), "%s.%s", name, extensions[i]);

		StrLower(asset->path, asset->path);

		if (Fs_Exists(asset->path)) {
			return true;
		}
	}

	*asset->path = '\0';
	return false;
}

/**
 * @brief
 */
static _Bool Cm_ResolveStageAnimation(cm_stage_t *stage, cm_asset_context_t context) {

	if (!Cm_ResolveAsset(&stage->asset, context)) {
		Com_Warn("Failed to resolve animation asset %s\n", stage->asset.name);
		return false;
	}

	const size_t size = sizeof(cm_asset_t) * stage->animation.num_frames;
	stage->animation.frames = Mem_LinkMalloc(size, stage);

	char base[MAX_QPATH];
	g_strlcpy(base, stage->asset.name, sizeof(base));

	char *c = base + strlen(base) - 1;
	while (isdigit(*c)) {
		c--;
	}

	c++;

	int32_t start = (int32_t) strtol(c, NULL, 10);
	*c = '\0';

	for (int32_t i = 0; i < stage->animation.num_frames; i++) {

		cm_asset_t *frame = &stage->animation.frames[i];
		g_snprintf(frame->name, sizeof(frame->name), "%s%d", base, start + i);

		if (!Cm_ResolveAsset(frame, context)) {
			Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->asset.name);
			return false;
		}
	}

	return true;
}

/**
 * @brief
 */
static _Bool Cm_ResolveStageAssets(cm_material_t *material, cm_stage_t *stage, cm_asset_context_t context) {

	_Bool res = false;
	
	if (*stage->asset.name) {

		if (stage->flags & STAGE_ANIMATION) {
			res = Cm_ResolveStageAnimation(stage, context);
		} else {
			if (stage->flags & STAGE_FLARE) {
				res = Cm_ResolveAsset(&stage->asset, ASSET_CONTEXT_SPRITES);
			} else {
				res = Cm_ResolveAsset(&stage->asset, context);
			}
		}

		if (res == false) {
			Com_Warn("Material %s stage %d: Failed to resolve asset(s) %s\n",
					 material->basename, Cm_StageIndex(material, stage), stage->asset.name);
		}
	} else {
		Com_Warn("Material %s stage %d: Stage does not specify an asset\n",
				 material->basename, Cm_StageIndex(material, stage));
	}

	return res;
}

/**
 * @brief Resolves the asset for the given material.
 */
static _Bool Cm_ResolveMaterialAsset(cm_material_t *material, cm_asset_t *asset, cm_asset_context_t context, const char **suffix) {

	if (*asset->name) {
		return Cm_ResolveAsset(asset, context);
	}

	for (const char **s = suffix; *s; s++) {
		g_snprintf(asset->name, sizeof(asset->name), "%s%s", material->basename, *s);
		if (Cm_ResolveAsset(asset, context)) {
			Com_Debug(DEBUG_COLLISION, "Resolved %s for %s\n", asset->path, material->name);
			break;
		}
	}

	*asset->name = '\0';
	return *asset->path != '\0';
}

/**
 * @brief Filesystem enumerator for resolving footstep assets.
 */
static void Cm_ResolveFootsteps_Enumerate(const char *file, void *data) {

	cm_footsteps_t *footsteps = data;

	cm_asset_t *out = footsteps->samples + footsteps->num_samples;

	out->name[0] = '#';
	g_strlcat(out->name, file, sizeof(out->name));
	g_strlcpy(out->path, file, sizeof(out->path));

	footsteps->num_samples++;
}

/**
 * @brief Comparator for sorting footstep assets.
 */
static int32_t Cm_ResolveFootsteps_Compare(const void *a, const void *b) {

	const cm_asset_t *a_asset = a;
	const cm_asset_t *b_asset = b;

	return g_strcmp0(a_asset->name, b_asset->name);
}

/**
 * @brief Resolves
 */
static void Cm_ResolveFootsteps(cm_footsteps_t *footsteps) {

	if (!strlen(footsteps->name)) {
		g_strlcpy(footsteps->name, "default", sizeof(footsteps->name));
	}

	const char *pattern = va("players/common/step_%s_*", footsteps->name);

	Fs_Enumerate(pattern, Cm_ResolveFootsteps_Enumerate, footsteps);

	if (!footsteps->num_samples) {
		Com_Warn("Footsteps \"%s\" have no samples\n", footsteps->name);
	} else {
		qsort(footsteps->samples, footsteps->num_samples, sizeof(cm_asset_t), Cm_ResolveFootsteps_Compare);
	}
}

/**
 * @brief Resolves all asset references within the specified material.
 */
_Bool Cm_ResolveMaterial(cm_material_t *material, cm_asset_context_t context) {

	assert(material);

	if (!Cm_ResolveMaterialAsset(material, &material->diffusemap, context, (const char *[]) { "", "_d", NULL })) {
		return false;
	}

	Cm_ResolveMaterialAsset(material, &material->normalmap, context, (const char *[]) { "_nm", "_norm", "_local", "_bump", NULL });
	Cm_ResolveMaterialAsset(material, &material->specularmap, context, (const char *[]) { "_s", "_spec", "_g", "_gloss", NULL });
	Cm_ResolveMaterialAsset(material, &material->tintmap, context, (const char *[]) { "_tint", NULL });

	cm_stage_t *stage = material->stages;
	while (stage) {
		if (Cm_ResolveStageAssets(material, stage, context)) {
			stage = stage->next;
		} else {
			return false;
		}
	}

	Cm_ResolveFootsteps(&material->footsteps);

	return true;
}

/**
 * @brief Serialize the given stage.
 */
static void Cm_WriteStage(const cm_material_t *material, const cm_stage_t *stage, file_t *file) {
	Fs_Print(file, "\t{\n");

	if (stage->flags & STAGE_TEXTURE) {
		Fs_Print(file, "\t\ttexture %s\n", stage->asset.name);
	}

	if (stage->flags & STAGE_BLEND) {
		Fs_Print(file, "\t\tblend %s %s\n", Cm_BlendNameByConst(stage->blend.src), Cm_BlendNameByConst(stage->blend.dest));
	}

	if (stage->flags & STAGE_COLOR) {
		Fs_Print(file, "\t\tcolor %g %g %g %g\n", stage->color.r, stage->color.g, stage->color.b, stage->color.a);
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
		Fs_Print(file, "\t\tdirtmap %g\n", stage->dirtmap.intensity);
	}

	if (stage->flags & STAGE_ENVMAP) {
		Fs_Print(file, "\t\tenvmap %s\n", stage->asset.name);
	}

	if (stage->flags & STAGE_WARP) {
		Fs_Print(file, "\t\twarp %g %g\n", stage->warp.hz, stage->warp.amplitude);
	}

	if (stage->flags & STAGE_SHELL) {
		Fs_Print(file, "\t\tshell %g\n", stage->shell.radius);
	}

	if (stage->flags & STAGE_FLARE) {
	   Fs_Print(file, "\t\tflare %s\n", stage->asset.name);
   }

	if (stage->flags & STAGE_ANIMATION) {
		Fs_Print(file, "\t\tanim %u %g\n", stage->animation.num_frames, stage->animation.fps);
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
	Fs_Print(file, "\tmaterial %s\n", material->name);

	if (*material->normalmap.name) {
		Fs_Print(file, "\tnormalmap %s\n", material->normalmap.name);
	}
	if (*material->specularmap.name) {
		Fs_Print(file, "\tspecularmap %s\n", material->specularmap.name);
	}
	if (*material->tintmap.name) {
		Fs_Print(file, "\ttintmap %s\n", material->tintmap.name);
	}

	Fs_Print(file, "\troughness %g\n", material->roughness);
	Fs_Print(file, "\thardness %g\n", material->hardness);
	Fs_Print(file, "\tspecularity %g\n", material->specularity);
	Fs_Print(file, "\tbloom %g\n", material->bloom);

	if (material->contents) {
		Fs_Print(file, "\tcontents \"%s\"\n", Cm_UnparseContents(material->contents));
	}

	if (material->surface) {
		Fs_Print(file, "\tsurface \"%s\"\n", Cm_UnparseSurface(material->surface));
	}

	if (material->alpha_test) {
		Fs_Print(file, "\talpha_test %g\n", material->alpha_test);
	}

	if (material->patch_size) {
		Fs_Print(file, "\tpatch_size %g\n", material->patch_size);
	}

	// if not empty/default, write footsteps
	if (*material->footsteps.name && g_strcmp0(material->footsteps.name, "default")) {
		Fs_Print(file, "\tfootsteps %s\n", material->footsteps.name);
	}

	if (material->light.radius) {
		Fs_Print(file, "\tlight.radius %g\n", material->light.radius);
	}

	if (material->light.intensity) {
		Fs_Print(file, "\tlight.intensity %g\n", material->light.intensity);
	}

	if (material->light.cone) {
		Fs_Print(file, "\tlight.cone %g\n", material->light.cone);
	}

	// write stages
	for (cm_stage_t *stage = material->stages; stage; stage = stage->next) {
		Cm_WriteStage(material, stage, file);
	}

	Fs_Print(file, "}\n");
}

/**
 * @brief GCompareFunc for Cm_WriteMaterials.
 */
static gint Cm_WriteMaterials_compare(gconstpointer a, gconstpointer b) {
	return g_strcmp0(((const cm_material_t *) a)->name, ((const cm_material_t *) b)->name);
}

/**
 * @brief Serialize the material(s) into the specified file.
 */
ssize_t Cm_WriteMaterials(const char *path, GList *materials) {

	ssize_t count = -1;
	file_t *file = Fs_OpenWrite(path);
	if (file) {
		count = 0;

		GList *sorted = g_list_sort(g_list_copy(materials), Cm_WriteMaterials_compare);

		for (const GList *list = sorted; list; list = list->next, count++) {
			Cm_WriteMaterial((cm_material_t *) list->data, file);
		}

		g_list_free(sorted);

		Fs_Close(file);
		Com_Print("Wrote %" PRIuPTR " materials to %s\n", count, path);
	} else {
		Com_Warn("Failed to open %s for write\n", path);
	}

	return count;
}
