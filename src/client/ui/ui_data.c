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
#include "client.h"

extern cl_static_t cls;

/**
 * @brief Callback setting a cvar_t's string.
 */
static void TW_CALL Ui_CvarSetString(const void *value, void *data) {
	cvar_t *var = (cvar_t *) data;
	const char *val = (const char *) value;

	Cvar_Set(var->name, val);
}

/**
 * @brief Callback exposing a cvar_t's string.
 */
static void TW_CALL Ui_CvarGetString(void *value, void *data) {
	cvar_t *var = (cvar_t *) data;
	char *val = (char *) value;

	strcpy(val, var->string);
}

/**
 * @brief Exposes a cvar_t as a text input accepting strings.
 */
void Ui_CvarText(TwBar *bar, const char *name, cvar_t *var, const char *def) {
	TwAddVarCB(bar, name, TW_TYPE_CSSTRING(128), Ui_CvarSetString, Ui_CvarGetString, var, def);
}

/**
 * @brief Callback setting a cvar_t's integer.
 */
static void TW_CALL Ui_CvarSetInteger(const void *value, void *data) {
	cvar_t *var = (cvar_t *) data;

	Cvar_Set(var->name, va("%d", *(int32_t *) value));
}

/**
 * @brief Callback exposing a cvar_t's integer.
 */
static void TW_CALL Ui_CvarGetInteger(void *value, void *data) {
	cvar_t *var = (cvar_t *) data;
	*(int32_t *) value = var->integer;
}

/**
 * @brief Exposes a cvar_t as a text input accepting integers.
 */
void Ui_CvarInteger(TwBar *bar, const char *name, cvar_t *var, const char *def) {
	TwAddVarCB(bar, name, TW_TYPE_INT32, Ui_CvarSetInteger, Ui_CvarGetInteger, var, def);
}

/**
 * @brief Exposes a cvar_t as a select input with predefined numeric values.
 */
void Ui_CvarEnum(TwBar *bar, const char *name, cvar_t *var, TwType en, const char *def) {
	TwAddVarCB(bar, name, en, Ui_CvarSetInteger, Ui_CvarGetInteger, var, def);
}

/**
 * @brief Callback setting a cvar_t's value.
 */
static void TW_CALL Ui_CvarSetValue(const void *value, void *data) {
	cvar_t *var = (cvar_t *) data;

	Cvar_Set(var->name, va("%f", *(vec_t *) value));
}

/**
 * @brief Callback exposing a cvar_t's value.
 */
static void TW_CALL Ui_CvarGetValue(void *value, void *data) {
	cvar_t *var = (cvar_t *) data;
	*(vec_t *) value = var->value;
}

/**
 * @brief Exposes a cvar_t as a text input accepting decimals.
 */
void Ui_CvarDecimal(TwBar *bar, const char *name, cvar_t *var, const char *def) {
	TwAddVarCB(bar, name, TW_TYPE_FLOAT, Ui_CvarSetValue, Ui_CvarGetValue, var, def);
}

/**
 * @brief Stuffs the specified text onto the command buffer.
 */
void TW_CALL Ui_Command(void *data) {
	Cbuf_AddText((const char *) data);
}

/**
 * @brief Binds the key specified in value to the command specified in data.
 * Because key binding bars are text-based, the key codes are serialized as
 * strings (e.g. "97" for 'a'). We also have to reverse-translate for
 * AntTweakBar, back to SDL.
 */
static void TW_CALL Ui_BindSet(const void *value, void *data) {

	const int32_t raw = strtol((const char *) value, NULL, 0);

	SDL_Keycode sym = raw;
	SDL_Scancode key = SDL_SCANCODE_UNKNOWN;

	switch (raw) {
		case TW_KEY_BACKSPACE:
			sym = SDLK_BACKSPACE;
			break;
		case TW_KEY_TAB:
			sym = SDLK_TAB;
			break;
		case TW_KEY_CLEAR:
			sym = SDLK_CLEAR;
			break;
		case TW_KEY_RETURN:
			sym = SDLK_RETURN;
			break;
		case TW_KEY_PAUSE:
			sym = SDLK_PAUSE;
			break;
		case TW_KEY_ESCAPE:
			sym = SDLK_ESCAPE;
			break;
		case TW_KEY_SPACE:
			sym = SDLK_SPACE;
			break;
		case TW_KEY_DELETE:
			sym = SDLK_DELETE;
			break;
		case TW_KEY_UP:
			sym = SDLK_UP;
			break;
		case TW_KEY_DOWN:
			sym = SDLK_DOWN;
			break;
		case TW_KEY_RIGHT:
			sym = SDLK_RIGHT;
			break;
		case TW_KEY_LEFT:
			sym = SDLK_LEFT;
			break;
		case TW_KEY_INSERT:
			sym = SDLK_INSERT;
			break;
		case TW_KEY_HOME:
			sym = SDLK_HOME;
			break;
		case TW_KEY_END:
			sym = SDLK_END;
			break;
		case TW_KEY_PAGE_UP:
			sym = SDLK_PAGEUP;
			break;
		case TW_KEY_PAGE_DOWN:
			sym = SDLK_PAGEDOWN;
			break;

		case TW_KEY_F1:
		case TW_KEY_F2:
		case TW_KEY_F3:
		case TW_KEY_F4:
		case TW_KEY_F5:
		case TW_KEY_F6:
		case TW_KEY_F7:
		case TW_KEY_F8:
		case TW_KEY_F9:
		case TW_KEY_F10:
		case TW_KEY_F11:
		case TW_KEY_F12:
			sym = raw - TW_KEY_F1 + SDLK_F1;
			break;

		case SDLK_MOUSE1:
		case SDLK_MOUSE2:
		case SDLK_MOUSE3:
		case SDLK_MOUSE4:
		case SDLK_MOUSE5:
		case SDLK_MOUSE6:
		case SDLK_MOUSE7:
		case SDLK_MOUSE8:
			key = raw & ~SDLK_SCANCODE_MASK;
			break;

		default:
			sym = raw;
			break;
	}

	if (key == SDL_SCANCODE_UNKNOWN) {
		key = SDL_GetScancodeFromKey(sym);
		if (key == SDL_SCANCODE_UNKNOWN)
			return;
	}

	Com_Debug("raw %d -> keysym %d -> scancode %d\n", raw, sym, key);

	_Bool did_unbind = false;

	if (sym == SDLK_BACKSPACE || sym == SDLK_DELETE) {
		char **binds = cls.key_state.binds;
		char *bind = (char *) data;

		for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
			if (binds[k] && !g_ascii_strcasecmp(bind, binds[k])) {
				Cl_Bind(k, NULL);
				did_unbind = true;
			}
		}
	}

	if (did_unbind == false) {
		Cl_Bind(key, (char *) data);
	}
}

/**
 * @brief Copies the key names for the bind specified in data to value.
 */
static void TW_CALL Ui_BindGet(void *value, void *data) {
	char **binds = cls.key_state.binds;
	char *bind = (char *) data;

	char *s = (char *) value;
	*s = '\0';

	for (SDL_Scancode k = SDL_SCANCODE_UNKNOWN; k < SDL_NUM_SCANCODES; k++) {
		if (binds[k] && !g_ascii_strcasecmp(bind, binds[k])) {
			strcat(s, va(strlen(s) ? ", %s" : "%s", Cl_KeyName(k)));
		}
	}
}

/**
 * @brief Exposes a key binding via the specified TwBar.
 */
void Ui_Bind(TwBar *bar, const char *name, const char *bind, const char *def) {
	TwAddVarCB(bar, name, TW_TYPE_BIND, Ui_BindSet, Ui_BindGet, (void *) bind, def);
}
