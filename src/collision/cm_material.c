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
#include "parse.h"

/**
 * @brief Free the memory for the specified material.
 */
void Cm_FreeMaterial(cm_material_t *material) {

	Mem_Free(material);
}

/**
 * @brief
 */
static int32_t Cm_ParseContents(const char *c) {

	int32_t contents = 0;

	if (strstr(c, "window")) {
		contents |= CONTENTS_WINDOW;
	}

	if (strstr(c, "lava")) {
		contents |= CONTENTS_LAVA;
	}

	if (strstr(c, "slime")) {
		contents |= CONTENTS_SLIME;
	}

	if (strstr(c, "water")) {
		contents |= CONTENTS_WATER;
	}

	if (strstr(c, "mist")) {
		contents |= CONTENTS_MIST;
	}

	if (strstr(c, "detail")) {
		contents |= CONTENTS_DETAIL;
	}

	if (strstr(c, "ladder")) {
		contents |= CONTENTS_LADDER;
	}

	return contents;
}

/**
 * @brief
 */
static char *Cm_UnparseContents(int32_t contents) {
	static char s[MAX_STRING_CHARS];
	*s = '\0';

	if (contents & CONTENTS_WINDOW) {
		g_strlcat(s, "window ", sizeof(s));
	}

	if (contents & CONTENTS_LAVA) {
		g_strlcat(s, "lava ", sizeof(s));
	}

	if (contents & CONTENTS_SLIME) {
		g_strlcat(s, "slime ", sizeof(s));
	}

	if (contents & CONTENTS_WATER) {
		g_strlcat(s, "water ", sizeof(s));
	}

	if (contents & CONTENTS_MIST) {
		g_strlcat(s, "mist ", sizeof(s));
	}

	if (contents & CONTENTS_DETAIL) {
		g_strlcat(s, "detail ", sizeof(s));
	}

	if (contents & CONTENTS_LADDER) {
		g_strlcat(s, "ladder ", sizeof(s));
	}

	return g_strchomp(s);
}

/**
 * @brief
 */
static int32_t Cm_ParseSurface(const char *c) {

	int32_t surface = 0;

	if (strstr(c, "light")) {
		surface |= SURF_LIGHT;
	}

	if (strstr(c, "slick")) {
		surface |= SURF_SLICK;
	}

	if (strstr(c, "sky")) {
		surface |= SURF_SKY;
	}

	if (strstr(c, "warp")) {
		surface |= SURF_WARP;
	}

	if (strstr(c, "blend_33")) {
		surface |= SURF_BLEND_33;
	}

	if (strstr(c, "blend_66")) {
		surface |= SURF_BLEND_66;
	}

	if (strstr(c, "no_draw")) {
		surface |= SURF_NO_DRAW;
	}

	if (strstr(c, "hint")) {
		surface |= SURF_HINT;
	}

	if (strstr(c, "skip")) {
		surface |= SURF_SKIP;
	}

	if (strstr(c, "alpha_test")) {
		surface |= SURF_ALPHA_TEST;
	}

	if (strstr(c, "phong")) {
		surface |= SURF_PHONG;
	}

	if (strstr(c, "material")) {
		surface |= SURF_MATERIAL;
	}

	if (strstr(c, "no_weld")) {
		surface |= SURF_NO_WELD;
	}

	if (strstr(c, "debug_luxel")) {
		surface |= SURF_DEBUG_LUXEL;
	}

	return surface;
}

/**
 * @brief
 */
static char *Cm_UnparseSurface(int32_t surface) {
	static char s[MAX_STRING_CHARS];
	*s = '\0';

	if (surface & SURF_LIGHT) {
		g_strlcat(s, "light ", sizeof(s));
	}

	if (surface & SURF_SLICK) {
		g_strlcat(s, "slick ", sizeof(s));
	}

	if (surface & SURF_SKY) {
		g_strlcat(s, "sky ", sizeof(s));
	}

	if (surface & SURF_WARP) {
		g_strlcat(s, "warp ", sizeof(s));
	}

	if (surface & SURF_BLEND_33) {
		g_strlcat(s, "blend_33 ", sizeof(s));
	}

	if (surface & SURF_BLEND_66) {
		g_strlcat(s, "blend_66 ", sizeof(s));
	}

	if (surface & SURF_NO_DRAW) {
		g_strlcat(s, "no_draw ", sizeof(s));
	}

	if (surface & SURF_HINT) {
		g_strlcat(s, "hint ", sizeof(s));
	}

	if (surface & SURF_SKIP) {
		g_strlcat(s, "skip ", sizeof(s));
	}

	if (surface & SURF_ALPHA_TEST) {
		g_strlcat(s, "alpha_test ", sizeof(s));
	}

	if (surface & SURF_PHONG) {
		g_strlcat(s, "phong ", sizeof(s));
	}

	if (surface & SURF_MATERIAL) {
		g_strlcat(s, "material ", sizeof(s));
	}

	return g_strchomp(s);
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
static void Cm_MaterialWarn(const char *path, const parser_t *parser, const char *message) {
	Com_Warn("%s: Syntax error (Ln %u Col %u)\n", path, parser->position.row + 1, parser->position.col);

	if (message) {
		Com_Warn("  %s\n", message);
	}
}

/**
 * @brief
 */
static int32_t Cm_ParseStage(cm_material_t *m, cm_stage_t *s, parser_t *parser, const char *path) {
	char token[MAX_TOKEN_CHARS];

	while (true) {

		if (!Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "texture") || !g_strcmp0(token, "diffuse")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->asset.name, sizeof(s->asset.name))) {
				Cm_MaterialWarn(path, parser, "Missing path or too many characters");
				continue;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!g_strcmp0(token, "envmap")) {

			if (!Parse_Token(parser, PARSE_NO_WRAP, s->asset.name, sizeof(s->asset.name))) {
				Cm_MaterialWarn(path, parser, "Missing path or too many characters");
				continue;
			}

			s->flags |= STAGE_ENVMAP;
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
			}

			if (!Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, parser, "Missing blend dest");
				continue;
			}

			s->blend.dest = Cm_BlendConstByName(token);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Cm_MaterialWarn(path, parser, "Invalid blend dest");
			}

			if (s->blend.src != GL_INVALID_ENUM &&
				s->blend.dest != GL_INVALID_ENUM) {
				s->flags |= STAGE_BLEND;
			}

			continue;
		}

		if (!g_strcmp0(token, "color")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, s->color.xyz, 3) != 3) {
				Cm_MaterialWarn(path, parser, "Need 3 values for color");
				continue;
			}

			for (int32_t i = 0; i < 3; i++) {

				if (s->color.xyz[i] < 0.0 || s->color.xyz[i] > 1.0) {
					Cm_MaterialWarn(path, parser, "Invalid value for color, must be between 0.0 and 1.0");
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!g_strcmp0(token, "pulse")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->pulse.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for pulse");
				continue;
			}

			if (s->pulse.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_PULSE;
			}

			continue;
		}

		if (!g_strcmp0(token, "stretch")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.amp, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for amplitude");
				continue;
			}

			if (s->stretch.amp == 0.0) {
				Cm_MaterialWarn(path, parser, "Amplitude must not be zero");
			}

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->stretch.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for frequency");
				continue;
			}

			if (s->stretch.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			}

			if (s->stretch.amp != 0.0 &&
				s->stretch.hz != 0.0) {
				s->flags |= STAGE_STRETCH;
			}

			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->rotate.hz, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for rotate");
				continue;
			}

			if (s->rotate.hz == 0.0) {
				Cm_MaterialWarn(path, parser, "Frequency must not be zero");
			} else {
				s->flags |= STAGE_ROTATE;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.s")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.s, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for scroll.s");
				continue;
			}

			if (s->scroll.s == 0.0) {
				Cm_MaterialWarn(path, parser, "scroll.s must not be zero");
			} else {
				s->flags |= STAGE_SCROLL_S;
			}

			continue;
		}

		if (!g_strcmp0(token, "scroll.t")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->scroll.t, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for scroll.t");
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
				Cm_MaterialWarn(path, parser, "Need a value for scale.s");
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
				Cm_MaterialWarn(path, parser, "Need a value for scale.t");
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
				s->terrain.height = s->terrain.ceil - s->terrain.floor;
				s->flags |= STAGE_TERRAIN;
			}

			continue;
		}

		if (!g_strcmp0(token, "dirtmap")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->dirt.intensity, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need a value for dirtmap");
				continue;
			}

			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Cm_MaterialWarn(path, parser, "Dirt intensity must be between 0.0 and 1.0");
			} else {
				s->flags |= STAGE_DIRTMAP;
			}

			continue;
		}

		if (!g_strcmp0(token, "anim")) {

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_UINT16, &s->anim.num_frames, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need number of frames");
				continue;
			}

			if (s->anim.num_frames < 1) {
				Cm_MaterialWarn(path, parser, "Invalid number of frames");
			}

			if (Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &s->anim.fps, 1) != 1) {
				Cm_MaterialWarn(path, parser, "Need FPS value");
				continue;
			}

			if (s->anim.fps < 0.0) {
				Cm_MaterialWarn(path, parser, "Invalid FPS value, must be >= 0.0");
			}

			// the frame images are loaded once the stage is parsed completely
			if (s->anim.num_frames && s->anim.fps >= 0.0) {
				s->flags |= STAGE_ANIM;
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

		if (!g_strcmp0(token, "mesh_color")) {
			s->mesh_color = true;
			continue;
		}

		if (*token == '}') {

			// a texture, or envmap means render it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP)) {
				s->flags |= STAGE_DIFFUSE;

				// a terrain blend or dirtmap means light it
				if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP)) {
					s->flags |= STAGE_LIGHTING;
				}
			}

			// a blend dest other than GL_ONE should use fog by default
			if (s->blend.dest != GL_ONE) {
				s->flags |= STAGE_FOG;
			}

			// determine if a numeric asset index was requested
			if (g_ascii_isdigit(s->asset.name[0])) {
				s->asset.index = (int32_t) strtol(s->asset.name, NULL, 0);
			} else {
				s->asset.index = -1;
			}

			Com_Debug(DEBUG_COLLISION,
			          "Parsed stage\n"
			          "  flags: %d\n"
			          "  texture: %s\n"
			          "   -> material: %s\n"
			          "  blend: %d %d\n"
			          "  color: %.1f %.1f %.1f\n"
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
			          ((s->flags & STAGE_LIGHTING) ? "true" : "false"), s->blend.src,
			          s->blend.dest, s->color.x, s->color.y, s->color.z, s->pulse.hz,
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
 * @brief Allocates a material, setting up the diffuse stage.
 */
cm_material_t *Cm_AllocMaterial(const char *name) {

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL diffuse name\n");
	}

	cm_material_t *mat = Mem_TagMalloc(sizeof(cm_material_t), MEM_TAG_MATERIAL);

	StripExtension(name, mat->name);

	Cm_MaterialBasename(mat->name, mat->basename, sizeof(mat->basename));

	mat->bump = DEFAULT_BUMP;
	mat->hardness = DEFAULT_HARDNESS;
	mat->parallax = DEFAULT_PARALLAX;
	mat->specular = DEFAULT_SPECULAR;

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

	parser_t parser;
	Parse_Init(&parser, (const char *) buf, PARSER_C_LINE_COMMENTS | PARSER_C_BLOCK_COMMENTS);

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

		if (!g_strcmp0(token, "material")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, MAX_QPATH)) {
				Cm_MaterialWarn(path, &parser, "Too many characters in path");
				break;
			}

			m = Cm_AllocMaterial(token);
			assert(m);

			for (const GList *list = *materials; list; list = list->next) {
				const cm_material_t *mat = list->data;
				if (!g_strcmp0(m->basename, mat->basename)) {
					Cm_MaterialWarn(path, &parser, "Redefining material");
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

		if (!g_strcmp0(token, "glossmap")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->glossmap.name, sizeof(m->glossmap.name))) {
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
			vec4_t color = Vec4_One();

			if (!g_strcmp0(token, "tintmap.tint_r_default")) {
				color = m->tintmap_defaults[TINT_R];
			} else if (!g_strcmp0(token, "tintmap.tint_g_default")) {
				color = m->tintmap_defaults[TINT_G];
			} else if (!g_strcmp0(token, "tintmap.tint_b_default")) {
				color = m->tintmap_defaults[TINT_B];
			} else {
				Cm_MaterialWarn(path, &parser, va("Invalid token \"%s\"", token));
			}

			const size_t num_parsed = Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, color.xyzw, 4);
			if (num_parsed < 3 || num_parsed > 4) {
				Cm_MaterialWarn(path, &parser, "Invalid color (must be 3 or 4 components)");
			} else {
				if (num_parsed != 4) {
					color.w = 1.0;
				}

				for (size_t i = 0; i < num_parsed; i++) {
					if (color.xyzw[i] < 0.0 || color.xyzw[i] > 1.0) {
						Cm_MaterialWarn(path, &parser, "Color number out of range (must be between 0.0 and 1.0)");
					}
				}
			}

			continue;
		}

		if (!g_strcmp0(token, "bump")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->bump, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->bump < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid bump value, must be > 0.0\n");
				m->bump = DEFAULT_BUMP;
			}
		}

		if (!g_strcmp0(token, "parallax")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->parallax, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->parallax < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid parallax value, must be > 0.0\n");
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if (!g_strcmp0(token, "hardness")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->hardness, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->hardness < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid hardness value, must be > 0.0\n");
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(token, "specular")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->specular, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No bump specified");
			} else if (m->specular < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid specular value, must be > 0.0\n");
				m->specular = DEFAULT_HARDNESS;
			}
		}

		if (!g_strcmp0(token, "contents")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No contents specified\n");
			} else {
				m->contents = Cm_ParseContents(token);
			}
			continue;
		}

		if (!g_strcmp0(token, "surface")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, token, sizeof(token))) {
				Cm_MaterialWarn(path, &parser, "No surface flags specified\n");
			} else {
				m->surface = Cm_ParseSurface(token);
			}
			continue;
		}

		if (!g_strcmp0(token, "light")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->light, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "No light specified");
				m->light = DEFAULT_LIGHT;
			} else if (m->light < 0.0) {
				Cm_MaterialWarn(path, &parser, "Invalid light value, must be > 0.0\n");
				m->light = DEFAULT_LIGHT;
			}

			m->surface |= SURF_LIGHT;
		}

		if (!g_strcmp0(token, "warp")) {

			if (Parse_Primitive(&parser, PARSE_NO_WRAP, PARSE_FLOAT, &m->warp, 1) != 1) {
				Cm_MaterialWarn(path, &parser, "Warp value not specified");
				m->warp = DEFAULT_WARP;
			}

			m->surface |= SURF_WARP;
		}

		if (!g_strcmp0(token, "footsteps")) {

			if (!Parse_Token(&parser, PARSE_NO_WRAP, m->footsteps, sizeof(m->footsteps))) {
				Cm_MaterialWarn(path, &parser, "Invalid footsteps value\n");
			}
		}

		if (!g_strcmp0(token, "stages_only")) {
			m->only_stages = true;
		}

		if (*token == '{' && in_material) {

			cm_stage_t *s = (cm_stage_t *) Mem_LinkMalloc(sizeof(*s), m);

			if (Cm_ParseStage(m, s, &parser, path) == -1) {
				Com_Debug(DEBUG_COLLISION, "Couldn't load a stage in %s\n", m->name);
				Mem_Free(s);
				continue;
			}

			// append the stage to the chain
			Cm_AttachStage(m, s);

			m->flags |= s->flags;
			continue;
		}

		if (*token == '}' && in_material) {
			*materials = g_list_prepend(*materials, m);
			in_material = false;
			count++;

			Com_Debug(DEBUG_COLLISION, "Parsed material %s with %d stages\n", m->name, m->num_stages);

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
			case ASSET_CONTEXT_ENVMAPS:
				if (asset->index > -1) {
					g_snprintf(name, sizeof(name), "envmaps/envmap_%s", asset->name);
				} else if (!g_str_has_prefix(asset->name, "envmaps/")) {
					g_snprintf(name, sizeof(name), "envmaps/%s", asset->name);
				}
				break;
			case ASSET_CONTEXT_FLARES:
				if (asset->index > -1) {
					g_snprintf(name, sizeof(name), "flares/flare_%s", asset->name);
				} else if (!g_str_has_prefix(asset->name, "flares/")) {
					g_snprintf(name, sizeof(name), "flares/%s", asset->name);
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
static _Bool Cm_ResolveStageAnimation(cm_stage_t *stage, cm_asset_context_t type) {

	const size_t size = sizeof(cm_asset_t) * stage->anim.num_frames;
	stage->anim.frames = Mem_LinkMalloc(size, stage);

	char base[MAX_QPATH];
	g_strlcpy(base, stage->asset.name, sizeof(base));

	char *c = base + strlen(base) - 1;
	while (isdigit(*c)) {
		c--;
	}

	c++;

	int32_t start = (int32_t) strtol(c, NULL, 10);
	*c = '\0';

	for (uint16_t i = 0; i < stage->anim.num_frames; i++) {

		cm_asset_t *frame = &stage->anim.frames[i];
		g_snprintf(frame->name, sizeof(frame->name), "%s%d", base, start + i);

		if (!Cm_ResolveAsset(frame, type)) {
			Com_Warn("Failed to resolve frame: %d: %s\n", i, stage->asset.name);
			return false;
		}
	}

	return true;
}

/**
 * @brief
 */
static _Bool Cm_ResolveStage(cm_stage_t *stage, cm_asset_context_t context) {

	if (*stage->asset.name) {

		if (stage->flags & STAGE_ENVMAP) {
			context = ASSET_CONTEXT_ENVMAPS;
		} else if (stage->flags & STAGE_FLARE) {
			context = ASSET_CONTEXT_FLARES;
		}

		if (Cm_ResolveAsset(&stage->asset, context)) {
			if (stage->flags & STAGE_ANIM) {
				return Cm_ResolveStageAnimation(stage, context);
			} else {
				return true;
			}
		} else {
			Com_Warn("Failed to resolve asset %s for stage\n", stage->asset.name);
		}
	} else {
		Com_Warn("Stage does not specify an asset\n");
	}

	return false;
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
 * @brief Resolves all asset references within the specified material.
 */
_Bool Cm_ResolveMaterial(cm_material_t *material, cm_asset_context_t context) {

	assert(material);

	if (!Cm_ResolveMaterialAsset(material, &material->diffusemap, context, (const char *[]) { "", "_d", NULL })) {
		return false;
	}

	Cm_ResolveMaterialAsset(material, &material->normalmap, context, (const char *[]) { "_nm", "_norm", "_local", "_bump", NULL });
	Cm_ResolveMaterialAsset(material, &material->heightmap, context, (const char *[]) { "_h", "_height", NULL });
	Cm_ResolveMaterialAsset(material, &material->glossmap, context, (const char *[]) { "_g", "_gloss", NULL });
	Cm_ResolveMaterialAsset(material, &material->specularmap, context, (const char *[]) { "_s", "_spec", NULL });
	Cm_ResolveMaterialAsset(material, &material->tintmap, context, (const char *[]) { "_tint", NULL });

	cm_stage_t *stage = material->stages;
	while (stage) {
		if (Cm_ResolveStage(stage, context)) {
			stage = stage->next;
		} else {
			return false;
		}
	}

	return true;
}

/**
 * @brief Serialize the given stage.
 */
static void Cm_WriteStage(const cm_material_t *material, const cm_stage_t *stage, file_t *file) {
	Fs_Print(file, "\t{\n");

	if (stage->flags & STAGE_TEXTURE) {
		Fs_Print(file, "\t\ttexture %s\n", stage->asset.name);
	} else if (stage->flags & STAGE_ENVMAP) {
		if (stage->asset.index > -1) {
			Fs_Print(file, "\t\tenvmap %d\n", stage->asset.index);
		} else {
			Fs_Print(file, "\t\tenvmap %s\n", stage->asset.name);
		}
	} else if (stage->flags & STAGE_FLARE) {
		if (stage->asset.index > -1) {
			Fs_Print(file, "\t\tflare %d\n", stage->asset.index);
		} else {
			Fs_Print(file, "\t\tflare %s\n", stage->asset.name);
		}
	} else {
		Com_Warn("Material %s has a stage with no image?\n", material->diffusemap.name);
	}

	if (stage->flags & STAGE_BLEND) {
		Fs_Print(file, "\t\tblend %s %s\n", Cm_BlendNameByConst(stage->blend.src), Cm_BlendNameByConst(stage->blend.dest));
	}

	if (stage->flags & STAGE_COLOR) {
		Fs_Print(file, "\t\tcolor %g %g %g\n", stage->color.x, stage->color.y, stage->color.z);
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
	Fs_Print(file, "\tmaterial %s\n", material->name);

	if (*material->normalmap.name) {
		Fs_Print(file, "\tnormalmap %s\n", material->normalmap.name);
	}
	if (*material->glossmap.name) {
		Fs_Print(file, "\tglossmap %s\n", material->glossmap.name);
	}
	if (*material->specularmap.name) {
		Fs_Print(file, "\tspecularmap %s\n", material->specularmap.name);
	}
	if (*material->tintmap.name) {
		Fs_Print(file, "\ttintmap %s\n", material->tintmap.name);
	}

	Fs_Print(file, "\tbump %g\n", material->bump);
	Fs_Print(file, "\thardness %g\n", material->hardness);
	Fs_Print(file, "\tspecular %g\n", material->specular);
	Fs_Print(file, "\tparallax %g\n", material->parallax);

	if (material->contents) {
		Fs_Print(file, "\tcontents \"%s\"\n", Cm_UnparseContents(material->contents));
	}

	if (material->surface) {
		Fs_Print(file, "\tsurface \"%s\"\n", Cm_UnparseSurface(material->surface));
	}

	if (material->light) {
		Fs_Print(file, "\tlight %g\n", material->light);
	}

	// if not empty/default, write footsteps
	if (*material->footsteps && g_strcmp0(material->footsteps, "default")) {
		Fs_Print(file, "\tfootsteps %s\n", material->footsteps);
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
