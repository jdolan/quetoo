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

#include "ui_local.h"

typedef enum {
	UI_EDITOR_NONE,

	UI_EDITOR_MATERIAL_DIFFUSE,
	UI_EDITOR_MATERIAL_NORMALMAP,

	UI_EDITOR_MATERIAL_BUMP,
	UI_EDITOR_MATERIAL_HARDNESS,
	UI_EDITOR_MATERIAL_SPECULAR,
	UI_EDITOR_MATERIAL_PARALLAX,
} ui_editor_field_t;

typedef struct {
	r_material_t *material;
	uint32_t time;
} ui_editor_t;

static ui_editor_t ui_editor;

/**
 * @brief Trace from the view origin to resolve the material to edit.
 */
static void Ui_Editor_Think(void) {
	extern cl_static_t cls;
	vec3_t end;

	if (cls.state != CL_ACTIVE)
		return;

	if (ui_editor.time == quetoo.time)
		return;

	VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, end);

	cm_trace_t tr = Cl_Trace(r_view.origin, end, NULL, NULL, 0, MASK_SOLID);
	if (tr.fraction < 1.0) {
		ui_editor.material = R_LoadMaterial(va("textures/%s", tr.surface->name));
		if (!ui_editor.material) {
			Com_Debug("Failed to resolve %s\n", tr.surface->name);
		}
	} else {
		ui_editor.material = NULL;
	}

	ui_editor.time = quetoo.time;
}

/**
 * @brief Callback setting an editor field.
 */
static void TW_CALL Ui_Editor_SetValue(const void *value, void *data) {

	const ui_editor_field_t field = (ui_editor_field_t) data;

	if (field >= UI_EDITOR_MATERIAL_DIFFUSE && field <= UI_EDITOR_MATERIAL_PARALLAX) {
		r_material_t *mat = ui_editor.material;

		if (!mat)
			return;

		switch (field) {

			case UI_EDITOR_MATERIAL_BUMP:
				mat->bump = *(vec_t *) value;
				break;
			case UI_EDITOR_MATERIAL_HARDNESS:
				mat->hardness = *(vec_t *) value;
				break;
			case UI_EDITOR_MATERIAL_SPECULAR:
				mat->specular = *(vec_t *) value;
				break;
			case UI_EDITOR_MATERIAL_PARALLAX:
				mat->parallax = *(vec_t *) value;
				break;

			default:
				break;
		}
	}
}

/**
 * @brief Callback exposing an editor field.
 */
static void TW_CALL Ui_Editor_GetValue(void *value, void *data) {

	Ui_Editor_Think();

	const ui_editor_field_t field = (ui_editor_field_t) data;

	if (field >= UI_EDITOR_MATERIAL_DIFFUSE && field <= UI_EDITOR_MATERIAL_PARALLAX) {
		const r_material_t *mat = ui_editor.material;

		if (!mat)
			return;

		switch (field) {

			case UI_EDITOR_MATERIAL_DIFFUSE:
				g_strlcpy(value, Basename(mat->diffuse->media.name), MAX_QPATH);
				break;
			case UI_EDITOR_MATERIAL_NORMALMAP:
				if (mat->normalmap)
					g_strlcpy(value, Basename(mat->normalmap->media.name), MAX_QPATH);
				break;

			case UI_EDITOR_MATERIAL_BUMP:
				*(vec_t *) value = mat->bump;
				break;
			case UI_EDITOR_MATERIAL_HARDNESS:
				*(vec_t *) value = mat->hardness;
				break;
			case UI_EDITOR_MATERIAL_SPECULAR:
				*(vec_t *) value = mat->specular;
				break;
			case UI_EDITOR_MATERIAL_PARALLAX:
				*(vec_t *) value = mat->parallax;
				break;

			default:
				break;
		}
	}
}

/**
 * @brief Exposes a material property as a text input accepting floating point.
 */
void Ui_EditorField(TwBar *bar, ui_editor_field_t field, const char *name, const char *def) {

	switch (field) {

		case UI_EDITOR_MATERIAL_DIFFUSE:
		case UI_EDITOR_MATERIAL_NORMALMAP:
			TwAddVarCB(bar, name, TW_TYPE_CSSTRING(MAX_QPATH), NULL, Ui_Editor_GetValue,
					(void *) field, def);
			break;

		case UI_EDITOR_MATERIAL_BUMP:
		case UI_EDITOR_MATERIAL_HARDNESS:
		case UI_EDITOR_MATERIAL_SPECULAR:
		case UI_EDITOR_MATERIAL_PARALLAX:
			TwAddVarCB(bar, name, TW_TYPE_FLOAT, Ui_Editor_SetValue, Ui_Editor_GetValue,
					(void *) field, def);
			break;

		default:
			break;
	}
}

/**
 * @brief
 */
TwBar *Ui_Editor(void) {

	memset(&ui_editor, 0, sizeof(ui_editor));

	TwBar *bar = TwNewBar("Editor");

	Ui_EditorField(bar, UI_EDITOR_MATERIAL_DIFFUSE, "Diffuse", NULL);
	Ui_EditorField(bar, UI_EDITOR_MATERIAL_NORMALMAP, "Normalmap", NULL);

	Ui_EditorField(bar, UI_EDITOR_MATERIAL_BUMP, "Bump", "min=0 max=20 step=0.125");
	Ui_EditorField(bar, UI_EDITOR_MATERIAL_HARDNESS, "Hardness", "min=0 max=20 step=0.1");
	Ui_EditorField(bar, UI_EDITOR_MATERIAL_SPECULAR, "Specular", "min=0 max=20 step=0.1");
	Ui_EditorField(bar, UI_EDITOR_MATERIAL_PARALLAX, "Parallax", "min=0 max=10 step=0.1");

	TwAddSeparator(bar, NULL, NULL);

	TwAddButton(bar, "Save", Ui_Command, "r_save_materials\n", NULL);

	TwDefine("Editor size='350 164' alpha=200 iconifiable=false valueswidth=175 visible=false");

	return bar;
}
