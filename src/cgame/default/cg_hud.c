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

#include "cg_local.h"

#define HUD_COLOR_STAT			CON_COLOR_DEFAULT
#define HUD_COLOR_STAT_MED		CON_COLOR_YELLOW
#define HUD_COLOR_STAT_LOW		CON_COLOR_RED

#define HUD_PIC_HEIGHT			64

#define HUD_HEALTH_MED			75
#define HUD_HEALTH_LOW			25

#define HUD_ARMOR_MED			50
#define HUD_ARMOR_LOW			25

#define HUD_POWERUP_LOW			5

typedef struct cg_crosshair_s {
	char name[16];
	r_image_t *image;
	vec4_t color;
} cg_crosshair_t;

static cg_crosshair_t crosshair;

#define CENTER_PRINT_LINES 8
typedef struct cg_center_print_s {
	char lines[CENTER_PRINT_LINES][MAX_STRING_CHARS];
	uint16_t num_lines;
	uint32_t time;
} cg_center_print_t;

static cg_center_print_t center_print;

typedef struct {
	int16_t icon_index;
	int16_t item_index;
} cg_hud_weapon_t;

static cg_hud_weapon_t cg_hud_weapons[MAX_STAT_BITS];

#define WEAPON_SELECT_OFF				-1

typedef struct {

	struct {
		uint32_t time;
		int16_t pickup;
	} pulse;

	struct {
		uint32_t hit_sound_time;
	} damage;

	struct {
		uint32_t pickup_time;
		uint32_t damage_time;
		int16_t pickup;
	} blend;

	struct {
		int16_t tag, used_tag;
		uint32_t time;
		int16_t bits;
		int16_t num;
		_Bool has[MAX_STAT_BITS];
	} weapon;

	int16_t chase_target;
} cg_hud_locals_t;

static r_image_t *cg_select_weapon_image;
static r_image_t *cg_pickup_blend_image;
static r_image_t *cg_quad_blend_image;
static r_image_t *cg_damage_blend_image;

static cvar_t *cg_select_weapon_delay;
static cvar_t *cg_select_weapon_interval;

static cg_hud_locals_t cg_hud_locals;

/**
 * @brief Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(const r_pixel_t x, const r_pixel_t y, const vec_t scale,
                        const uint16_t icon) {

	if (icon >= MAX_IMAGES || !cgi.client->image_precache[icon]) {
		cgi.Warn("Invalid icon: %d\n", icon);
		return;
	}

	cgi.DrawImage(x, y, scale, cgi.client->image_precache[icon]);
}

/**
 * @brief Draws the vital numeric and icon, flashing on low quantities.
 */
static void Cg_DrawVital(r_pixel_t x, const int16_t value, const int16_t icon, int16_t med,
                         int16_t low) {
	r_pixel_t y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT + 4;

	vec4_t pulse = { 1.0, 1.0, 1.0, 1.0 };
	int32_t color = HUD_COLOR_STAT;

	if (value < low) {
		if (cg_draw_vitals_pulse->integer) {
			pulse[3] = sin(cgi.client->unclamped_time / 250.0) + 0.75;
		}
		color = HUD_COLOR_STAT_LOW;
	} else if (value < med) {
		color = HUD_COLOR_STAT_MED;
	}

	const char *string = va("%3d", value);

	x += cgi.view->viewport.x;
	cgi.DrawString(x, y, string, color);

	x += cgi.StringWidth(string);
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT;

	cgi.Color(pulse);
	Cg_DrawIcon(x, y, 1.0, icon);
	cgi.Color(NULL);
}

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(const player_state_t *ps) {
	r_pixel_t x, cw, x_offset;

	if (!cg_draw_vitals->integer) {
		return;
	}

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const int16_t health = ps->stats[STAT_HEALTH];
		const int16_t health_icon = ps->stats[STAT_HEALTH_ICON];

		x = cgi.view->viewport.w * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if (cg_state.gameplay != GAME_INSTAGIB) {

		if (ps->stats[STAT_AMMO] > 0) {
			const int16_t ammo = ps->stats[STAT_AMMO];
			const int16_t ammo_low = ps->stats[STAT_AMMO_LOW];
			const int16_t ammo_icon = ps->stats[STAT_AMMO_ICON];

			x = cgi.view->viewport.w * 0.5 - x_offset;

			Cg_DrawVital(x, ammo, ammo_icon, -1, ammo_low);
		}

		if (ps->stats[STAT_ARMOR] > 0) {
			const int16_t armor = ps->stats[STAT_ARMOR];
			const int16_t armor_icon = ps->stats[STAT_ARMOR_ICON];

			x = cgi.view->viewport.w * 0.75 - x_offset;

			Cg_DrawVital(x, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
		}
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the powerup and the time remaining
 */
static void Cg_DrawPowerup(r_pixel_t y, const int16_t value, const r_image_t *icon) {
	r_pixel_t x;

	vec4_t pulse = { 1.0, 1.0, 1.0, 1.0 };
	int32_t color = HUD_COLOR_STAT;

	if (value < HUD_POWERUP_LOW) {
		color = HUD_COLOR_STAT_LOW;
	}

	const char *string = va("%d", value);

	x = cgi.view->viewport.x + (HUD_PIC_HEIGHT / 2);

	cgi.Color(pulse);
	cgi.DrawImage(x, y, 1.0, icon);
	cgi.Color(NULL);

	x += HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, string, color);
}

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawPowerups(const player_state_t *ps) {
	r_pixel_t y, ch;

	if (!cg_draw_powerups->integer) {
		return;
	}

	cgi.BindFont("large", &ch, NULL);

	y = cgi.view->viewport.y + (cgi.view->viewport.h / 2);

	if (ps->stats[STAT_QUAD_TIME] > 0) {
		const int32_t timer = ps->stats[STAT_QUAD_TIME];

		Cg_DrawPowerup(y, timer, cgi.LoadImage("pics/i_quad", IT_PIC));

		y += HUD_PIC_HEIGHT;
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the flag you are currently holding
 */
static void Cg_DrawHeldFlag(const player_state_t *ps) {
	r_pixel_t x, y;

	if (!cg_draw_held_flag->integer) {
		return;
	}

	vec4_t pulse = { 1.0, 1.0, 1.0, sin(cgi.client->unclamped_time / 150.0) + 0.75 };

	x = cgi.view->viewport.x + (HUD_PIC_HEIGHT / 2);
	y = cgi.view->viewport.y + ((cgi.view->viewport.h / 2) - (HUD_PIC_HEIGHT * 2));

	uint16_t flag = ps->stats[STAT_CARRYING_FLAG];

	if (flag != 0) {
		cgi.Color(pulse);

		cgi.DrawImage(x, y, 1.0, cgi.LoadImage(va("pics/i_flag%d", flag), IT_PIC));

		cgi.Color(NULL);
	}
}

/**
 * @brief Draws the flag you are currently holding
 */
static void Cg_DrawHeldTech(const player_state_t *ps) {
	r_pixel_t x, y;

	if (!cg_draw_held_tech->integer) {
		return;
	}

	vec4_t pulse = { 1.0, 1.0, 1.0, 1.0 };

	x = cgi.view->viewport.x + 4;
	y = cgi.view->viewport.y + ((cgi.view->viewport.h / 2) - (HUD_PIC_HEIGHT * 4));

	int16_t tech = ps->stats[STAT_TECH_ICON];

	if (tech != -1) {
		cgi.Color(pulse);

		Cg_DrawIcon(x, y, 1.0, tech);

		cgi.Color(NULL);
	}
}

/**
 * @brief
 */
static void Cg_DrawPickup(const player_state_t *ps) {
	r_pixel_t x, y, cw, ch;

	if (!cg_draw_pickup->integer) {
		return;
	}

	cgi.BindFont(NULL, &cw, &ch);

	if (ps->stats[STAT_PICKUP_ICON] != -1) {
		const int16_t icon = ps->stats[STAT_PICKUP_ICON] & ~STAT_TOGGLE_BIT;
		const int16_t pickup = ps->stats[STAT_PICKUP_STRING];

		const char *string = cgi.ConfigString(pickup);

		x = cgi.view->viewport.x + cgi.view->viewport.w - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = cgi.view->viewport.y;

		Cg_DrawIcon(x, y, 1.0, icon);

		x += HUD_PIC_HEIGHT;
		y += (HUD_PIC_HEIGHT - ch) / 2 + 2;

		cgi.DrawString(x, y, string, HUD_COLOR_STAT);
	}
}

/**
 * @brief
 */
static void Cg_DrawFrags(const player_state_t *ps) {
	const int16_t frags = ps->stats[STAT_FRAGS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (!cg_draw_frags->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Frags");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, "Frags", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", frags), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawDeaths(const player_state_t *ps) {
	const int16_t deaths = ps->stats[STAT_DEATHS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (!cg_draw_deaths->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Deaths");
	y = cgi.view->viewport.y + 2 * (HUD_PIC_HEIGHT + ch);

	cgi.DrawString(x, y, "Deaths", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", deaths), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}


/**
 * @brief
 */
static void Cg_DrawCaptures(const player_state_t *ps) {
	const int16_t captures = ps->stats[STAT_CAPTURES];
	r_pixel_t x, y, cw, ch;

	if (!cg_draw_captures->integer) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	if (!cg_state.ctf) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Captures");
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	cgi.DrawString(x, y, "Captures", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->viewport.x + cgi.view->viewport.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", captures), HUD_COLOR_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawSpectator(const player_state_t *ps) {
	r_pixel_t x, y, cw;

	if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE]) {
		return;
	}

	cgi.BindFont("small", &cw, NULL);

	x = cgi.view->viewport.w - cgi.StringWidth("Spectating");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Spectating", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawChase(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_USER_INFO_VALUE * 2], *s;

	// if we've changed chase targets, reset all locals
	if (ps->stats[STAT_CHASE] != cg_hud_locals.chase_target) {
		memset(&cg_hud_locals, 0, sizeof(cg_hud_locals));
		cg_hud_locals.chase_target = ps->stats[STAT_CHASE];
	}

	if (!ps->stats[STAT_CHASE]) {
		return;
	}

	const int32_t c = ps->stats[STAT_CHASE];

	if (c - CS_CLIENTS >= MAX_CLIENTS) {
		cgi.Warn("Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	g_snprintf(string, sizeof(string), "Chasing ^7%s", cgi.ConfigString(c));

	if ((s = strchr(string, '\\'))) {
		*s = '\0';
	}

	x = cgi.view->viewport.x + cgi.view->viewport.w * 0.5 - cgi.StringWidth(string) / 2;
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawVote(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_STRING_CHARS];

	if (!cg_draw_vote->integer) {
		return;
	}

	if (!ps->stats[STAT_VOTE]) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	g_snprintf(string, sizeof(string), "Vote: ^7%s", cgi.ConfigString(ps->stats[STAT_VOTE]));

	x = cgi.view->viewport.x;
	y = cgi.view->viewport.y + cgi.view->viewport.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawTime(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	const char *string = cgi.ConfigString(CS_TIME);

	if (!ps->stats[STAT_TIME]) {
		return;
	}

	if (!cg_draw_time->integer) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth(string);
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	if (cg_state.ctf) {
		y += HUD_PIC_HEIGHT + ch;
	}

	cgi.DrawString(x, y, string, CON_COLOR_DEFAULT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawReady(const player_state_t *ps) {
	r_pixel_t x, y, ch;

	if (!ps->stats[STAT_READY]) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Ready");
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	if (cg_state.ctf > 0) {
		y += HUD_PIC_HEIGHT + ch;
	}

	y += ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawTeamBanner(const player_state_t *ps) {
	const int16_t team = ps->stats[STAT_TEAM];
	r_pixel_t x, y;

	if (team == -1) {
		return;
	}

	if (!cg_draw_team_banner->integer) {
		return;
	}

	color_t color = cg_team_info[team].color;
	color.a = 38;

	x = cgi.view->viewport.x;
	y = cgi.view->viewport.y + cgi.view->viewport.h - 64;

	cgi.DrawFill(x, y, cgi.view->viewport.w, 64, color.u32, -1.0);
}

/**
 * @brief
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {
	r_pixel_t x, y;

	if (!cg_draw_crosshair->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return; // spectating
	}

	if (center_print.time > cgi.client->unclamped_time) {
		return;
	}

	if (cg_draw_crosshair->modified) { // crosshair image
		cg_draw_crosshair->modified = false;

		if (cg_draw_crosshair->value < 0) {
			cg_draw_crosshair->value = 1;
		}

		if (cg_draw_crosshair->value > 100) {
			cg_draw_crosshair->value = 100;
		}

		g_snprintf(crosshair.name, sizeof(crosshair.name), "ch%d", cg_draw_crosshair->integer);

		crosshair.image = cgi.LoadImage(va("pics/ch%d", cg_draw_crosshair->integer), IT_PIC);

		if (crosshair.image->type == IT_NULL) {
			cgi.Print("Couldn't load pics/ch%d.\n", cg_draw_crosshair->integer);
			return;
		}
	}

	if (crosshair.image->type == IT_NULL) { // not found
		return;
	}

	if (cg_draw_crosshair_color->modified) { // crosshair color
		cg_draw_crosshair_color->modified = false;

		color_t color;
		if (!g_strcmp0(cg_draw_crosshair_color->string, "default")) {
			color.r = color.g = color.b = 255;
		} else {
			ColorFromHex(cg_draw_crosshair_color->string, &color);
		}

		ColorToVec4(color, crosshair.color);
	}

	vec_t alpha = 1.0, scale = cg_draw_crosshair_scale->value * (cgi.context->high_dpi ? 2.0 : 1.0);

	// pulse the crosshair size and alpha based on pickups
	if (cg_draw_crosshair_pulse->value) {

		const int16_t p = ps->stats[STAT_PICKUP_ICON];
		if (p != -1 && (p != cg_hud_locals.pulse.pickup)) {
			cg_hud_locals.pulse.time = cgi.client->unclamped_time;
		}

		cg_hud_locals.pulse.pickup = p;

		const uint32_t delta = cgi.client->unclamped_time - cg_hud_locals.pulse.time;
		if (delta < 300) {
			const vec_t frac = 1.0 - (delta / 300.0);
			scale += cg_draw_crosshair_pulse->value * frac;
			alpha += cg_draw_crosshair_pulse->value * frac;
		}

		crosshair.color[3] = alpha;
	}

	cgi.Color(crosshair.color);

	// calculate width and height based on crosshair image and scale
	x = (cgi.context->width - crosshair.image->width * scale) / 2.0;
	y = (cgi.context->height - crosshair.image->height * scale) / 2.0;

	cgi.DrawImage(x, y, scale, crosshair.image);

	cgi.Color(NULL);
}

/**
 * @brief
 */
void Cg_ParseCenterPrint(void) {
	char *c, *out, *line;

	memset(&center_print, 0, sizeof(center_print));

	c = cgi.ReadString();

	line = center_print.lines[0];
	out = line;

	while (*c && center_print.num_lines < CENTER_PRINT_LINES - 1) {

		if (*c == '\n') {
			line += MAX_STRING_CHARS;
			out = line;
			center_print.num_lines++;
			c++;
			continue;
		}

		*out++ = *c++;
	}

	center_print.num_lines++;
	center_print.time = cgi.client->unclamped_time + 3000;
}

/**
 * @brief
 */
static void Cg_DrawCenterPrint(const player_state_t *ps) {
	r_pixel_t cw, ch, x, y;
	char *line = center_print.lines[0];

	if (ps->stats[STAT_SCORES]) {
		return;
	}

	if (center_print.time < cgi.client->unclamped_time) {
		return;
	}

	cgi.BindFont(NULL, &cw, &ch);

	y = (cgi.context->height - center_print.num_lines * ch) / 2;

	while (*line) {
		x = (cgi.context->width - cgi.StringWidth(line)) / 2;

		cgi.DrawString(x, y, line, CON_COLOR_DEFAULT);
		line += MAX_STRING_CHARS;
		y += ch;
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Perform composition of the dst/src blends.
 */
static void Cg_AddBlend(vec4_t blend, const vec4_t input) {

	if (input[3] <= 0.0) {
		return;
	}

	vec4_t out;

	out[3] = input[3] + blend[3] * (1.0 - input[3]);

	for (int32_t i = 0; i < 3; i++) {
		out[i] = ((input[i] * input[3]) + ((blend[i] * blend[3]) * (1.0 - input[3]))) / out[3];
	}

	Vector4Copy(out, blend);
}

/**
 * @brief Perform composition of the src blend with the specified color palette index/alpha combo.
 */
static void Cg_AddBlendPalette(vec4_t blend, const uint8_t color, const vec_t alpha) {

	if (alpha <= 0.0) {
		return;
	}

	vec4_t blend_color;
	cgi.ColorFromPalette(color, blend_color);
	blend_color[3] = alpha;

	Cg_AddBlend(blend, blend_color);
}

/**
 * @brief Calculate the alpha factor for the specified blend components.
 * @param blend_start_time The start of the blend, in unclamped time.
 * @param blend_decay_time The length of the blend in milliseconds.
 * @param blend_alpha The base alpha value.
 */
static vec_t Cg_CalculateBlendAlpha(const uint32_t blend_start_time, const uint32_t blend_decay_time,
                                    const vec_t blend_alpha) {

	if ((cgi.client->unclamped_time - blend_start_time) <= blend_decay_time) {
		const vec_t time_factor = (vec_t) (cgi.client->unclamped_time - blend_start_time) / blend_decay_time;
		const vec_t alpha = cg_draw_blend->value * (blend_alpha - (time_factor * blend_alpha));

		return alpha;
	}

	return 0.0;
}

/**
 * @brief Draw a blend flash image with a specified alpha.
 * @param icon The picture to use
 * @param alpha The alpha of the blend
 */
static void Cg_DrawBlendFlashImage(const r_image_t *image, const vec_t alpha) {

	if (alpha <= 0.0) {
		return;
	}

	cgi.Color((const vec4_t) { 1.0, 1.0, 1.0, alpha });
	cgi.DrawImageResized(0, 0, cgi.context->width, cgi.context->height, image);
	cgi.Color(NULL);
}

#define CG_DAMAGE_BLEND_TIME 1500
#define CG_PICKUP_BLEND_TIME 600

/**
 * @brief Draw a full-screen blend effect based on world interaction.
 */
static void Cg_DrawBlend(const player_state_t *ps) {

	if (!cg_draw_blend->value) {
		return;
	}

	vec4_t blend = { 0.0, 0.0, 0.0, 0.0 };

	// start with base blend based on view origin conents

	const int32_t contents = cgi.view->contents;

	if ((contents & MASK_LIQUID) && cg_draw_blend_liquid->value) {
		uint8_t color;
		if (contents & CONTENTS_LAVA) {
			color = 71;
		} else if (contents & CONTENTS_SLIME) {
			color = 201;
		} else {
			color = 114;
		}

		Cg_AddBlendPalette(blend, color, 0.4 * cg_draw_blend_liquid->value);
	}

	// pickups

	const int16_t p = ps->stats[STAT_PICKUP_ICON] & ~STAT_TOGGLE_BIT;

	if (p > -1 && (p != cg_hud_locals.blend.pickup)) { // don't flash on same item
		cg_hud_locals.blend.pickup_time = cgi.client->unclamped_time;
	}

	cg_hud_locals.blend.pickup = p;

	if (cg_hud_locals.blend.pickup_time && cg_draw_blend_pickup->value) {
		Cg_DrawBlendFlashImage(cg_pickup_blend_image,
			Cg_CalculateBlendAlpha(cg_hud_locals.blend.pickup_time, CG_PICKUP_BLEND_TIME, cg_draw_blend_pickup->value));
	}

	// quad damage powerup

	if (ps->stats[STAT_QUAD_TIME] > 0 && cg_draw_blend_powerup->value) {
		Cg_DrawBlendFlashImage(cg_quad_blend_image,
			fabs(sin(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value);
	}

	// taken damage

	const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

	if (d) {
		cg_hud_locals.blend.damage_time = cgi.client->unclamped_time;
	}

	if (cg_hud_locals.blend.damage_time && cg_draw_blend_damage->value) {
		Cg_DrawBlendFlashImage(cg_damage_blend_image,
			Cg_CalculateBlendAlpha(cg_hud_locals.blend.damage_time, CG_DAMAGE_BLEND_TIME, cg_draw_blend_damage->value));
	}

	// if we have a blend, draw it

	if (blend[3] > 0.0) {
		color_t final_color;

		for (int32_t i = 0; i < 4; i++) {
			final_color.bytes[i] = (uint8_t) (blend[i] * 255.0);
		}

		cgi.DrawFill(cgi.view->viewport.x, cgi.view->viewport.y,
		             cgi.view->viewport.w, cgi.view->viewport.h, final_color.u32, -1.0);
	}
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
static void Cg_DrawDamageInflicted(const player_state_t *ps) {

	if (!cg_hit_sound->integer) {
		return;
	}

	const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
	if (dmg) {

		// play the hit sound
		if (cgi.client->unclamped_time - cg_hud_locals.damage.hit_sound_time > 50) {
			cg_hud_locals.damage.hit_sound_time = cgi.client->unclamped_time;

			cgi.AddSample(&(const s_play_sample_t) {
				.sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0]
			});
		}
	}
}

/**
 * @brief
 */
void Cg_ParseWeaponInfo(const char *s) {

	cgi.Debug("Received weapon info from server: %s\n", s);

	gchar **info = g_strsplit(s, "\\", 0);
	const size_t num_info = g_strv_length(info);

	if ((num_info / 2) > MAX_STAT_BITS || num_info & 1) {
		g_strfreev(info);
		cgi.Error("Invalid weapon info");
	}

	cg_hud_weapon_t *weapon = cg_hud_weapons;

	for (size_t i = 0; i < num_info; i += 2, weapon++) {
		weapon->icon_index = atoi(info[i]);
		weapon->item_index = atoi(info[i + 1]);
	}

	g_strfreev(info);
}

/**
 * @brief Return active weapon tag. This might be the weapon we're changing to, not the
 * one we have equipped.
 */
static int16_t Cg_ActiveWeapon(const player_state_t *ps) {

	int16_t switching = ((ps->stats[STAT_WEAPON_TAG] >> 8) & 0xFF);

	if (switching) {
		return switching - 1;
	}

	return (ps->stats[STAT_WEAPON_TAG] & 0xFF) - 1;
}

/**
 * @brief
 */
static void Cg_ValidateSelectedWeapon(const player_state_t *ps) {

	// if we were off, start from our current weapon.
	if (cg_hud_locals.weapon.tag == WEAPON_SELECT_OFF) {
		cg_hud_locals.weapon.tag = Cg_ActiveWeapon(ps);
		return;
	}

	// see if we have this weapon
	if (cg_hud_locals.weapon.has[cg_hud_locals.weapon.tag]) {
		return; // got it
	}

	// nope, so pick the closest one we have
	for (size_t i = 2; i < MAX_STAT_BITS * 2; i++) {
		int32_t offset = (int32_t) (((i & 1) ? -i : i) / 2);
		int32_t id = cg_hud_locals.weapon.tag + offset;

		if (id < 0 || id >= (int32_t) MAX_STAT_BITS) {
			continue;
		}

		if (cg_hud_locals.weapon.has[id]) {
			cg_hud_locals.weapon.tag = id;
			return;
		}
	}

	// should never happen
	cg_hud_locals.weapon.tag = WEAPON_SELECT_OFF;
}

/**
 * @brief
 */
static void Cg_SelectWeapon(const int8_t dir) {
	const player_state_t *ps = &cgi.client->frame.ps;

	if (ps->stats[STAT_SPECTATOR]) {

		if (ps->stats[STAT_CHASE]) {

			if (dir == 1) {
				cgi.Cbuf("chase_next");
			} else {
				cgi.Cbuf("chase_previous");
			}
		}

		return;
	}

	Cg_ValidateSelectedWeapon(ps);

	for (int16_t i = 0; i < (int16_t) MAX_STAT_BITS; i++) {

		cg_hud_locals.weapon.tag += dir;

		if (cg_hud_locals.weapon.tag < 0) {
			cg_hud_locals.weapon.tag = MAX_STAT_BITS - 1;
		} else if (cg_hud_locals.weapon.tag >= (int16_t) MAX_STAT_BITS) {
			cg_hud_locals.weapon.tag = 0;
		}

		if (cg_hud_locals.weapon.has[cg_hud_locals.weapon.tag]) {
			cg_hud_locals.weapon.time = cgi.client->unclamped_time + cg_select_weapon_delay->integer;
			return;
		}
	}

	// should never happen
	cg_hud_locals.weapon.tag = WEAPON_SELECT_OFF;
}

/**
 * @brief
 */
_Bool Cg_AttemptSelectWeapon(const player_state_t *ps) {

	cg_hud_locals.weapon.time = 0;

	if (!ps->stats[STAT_SPECTATOR] &&
		cg_hud_locals.weapon.tag != -1) {

		if (cg_hud_locals.weapon.tag != Cg_ActiveWeapon(ps)) {
			const char *name = cgi.client->config_strings[CS_ITEMS + cg_hud_weapons[cg_hud_locals.weapon.tag].item_index];
			cgi.Cbuf(va("use %s\n", name));

			cg_hud_locals.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
			return true;
		}

		cg_hud_locals.weapon.tag = -1;
		return true;
	}

	return false;
}

/**
 * @brief
 */
static void Cg_DrawSelectWeapon(const player_state_t *ps) {

	// spectator/dead
	if (!ps->stats[STAT_WEAPONS]) {
		cg_hud_locals.weapon.tag = -1;
		cg_hud_locals.weapon.time = 0;
		cg_hud_locals.weapon.used_tag = 0;
		return;
	}

	// see if we have any weapons at all
	if (cg_hud_locals.weapon.bits != ps->stats[STAT_WEAPONS])
	{
		cg_hud_locals.weapon.bits = ps->stats[STAT_WEAPONS];
		cg_hud_locals.weapon.num = 0;

		for (int32_t i = 0; i < (int32_t) MAX_STAT_BITS; i++) {
			cg_hud_locals.weapon.has[i] = !!(cg_hud_locals.weapon.bits & (1 << i));

			if (cg_hud_locals.weapon.has[i])
				cg_hud_locals.weapon.num++;
		}
	}

	if (!cg_hud_locals.weapon.num) {
		cg_hud_locals.weapon.tag = -1;
		cg_hud_locals.weapon.time = 0;
		cg_hud_locals.weapon.used_tag = 0;
		return;
	}

	int16_t switching = ((ps->stats[STAT_WEAPON_TAG] >> 8) & 0xFF);

	if (cg_hud_locals.weapon.used_tag != switching) {
		cg_hud_locals.weapon.used_tag = switching;

		if (cg_hud_locals.weapon.used_tag && !ps->stats[STAT_SPECTATOR]) {

			// we changed weapons without using scrolly, show it for a bit
			cg_hud_locals.weapon.tag = cg_hud_locals.weapon.used_tag - 1;
			cg_hud_locals.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
		}
	}

	// not changing or ran out of time
	if (cg_hud_locals.weapon.time <= cgi.client->unclamped_time) {
		Cg_AttemptSelectWeapon(ps);

		if (cg_hud_locals.weapon.time <= cgi.client->unclamped_time) {
			return;
		}
	}

	// figure out weapon.tag
	Cg_ValidateSelectedWeapon(ps);

	r_pixel_t x = cgi.view->viewport.x + ((cgi.view->viewport.w / 2) - ((cg_hud_locals.weapon.num * HUD_PIC_HEIGHT) / 2));
	r_pixel_t y = cgi.view->viewport.y + cgi.view->viewport.h - (HUD_PIC_HEIGHT * 2.0) - 16;

	r_pixel_t ch;
	cgi.BindFont("medium", NULL, &ch);

	for (int16_t i = 0; i < (int16_t) MAX_STAT_BITS; i++) {

		if (!cg_hud_locals.weapon.has[i]) {
			continue;
		}

		Cg_DrawIcon(x, y, 1.0, cg_hud_weapons[i].icon_index);

		if (i == cg_hud_locals.weapon.tag) {
			const char *name = cgi.client->config_strings[CS_ITEMS + cg_hud_weapons[i].item_index];
			cgi.DrawString(cgi.view->viewport.x + ((cgi.view->viewport.w / 2) - (cgi.StringWidth(name) / 2)), y - ch, name, HUD_COLOR_STAT);
			cgi.DrawImage(x, y, 1.0, cg_select_weapon_image);
		}

		x += HUD_PIC_HEIGHT + 4;
	}
}

/**
 * @brief
 */
static void Cg_DrawTargetName(const player_state_t *ps) {
	static uint32_t time;
	static char name[MAX_USER_INFO_VALUE];

	if (!cg_draw_target_name->integer) {
		return;
	}

	if (time > cgi.client->unclamped_time) {
		time = 0;
	}

	vec3_t pos;
	VectorMA(cgi.view->origin, MAX_WORLD_DIST, cgi.view->forward, pos);

	const cm_trace_t tr = cgi.Trace(cgi.view->origin, pos, NULL, NULL, 0, MASK_MEAT);
	if (tr.fraction < 1.0) {

		const cl_entity_t *ent = &cgi.client->entities[(ptrdiff_t) tr.ent];
		if (ent->current.model1 == MODEL_CLIENT) {

			const cl_client_info_t *client = &cgi.client->client_info[ent->current.number - 1];

			g_strlcpy(name, client->name, sizeof(name));
			time = cgi.client->unclamped_time;
		}
	}

	if (cgi.client->unclamped_time - time > 3000) {
		*name = '\0';
	}

	if (*name) {
		r_pixel_t ch;
		cgi.BindFont("medium", NULL, &ch);

		const r_pixel_t w = cgi.StringWidth(name);
		const r_pixel_t x = cgi.view->viewport.x + ((cgi.view->viewport.w / 2) - (w / 2));
		const r_pixel_t y = cgi.view->viewport.y + cgi.view->viewport.h - 192 - ch;

		cgi.DrawString(x, y, name, CON_COLOR_GREEN);
	}
}

/**
 * @brief
 */
static void Cg_Weapon_Next_f(void) {
	Cg_SelectWeapon(1);
}

/**
 * @brief
 */
static void Cg_Weapon_Prev_f(void) {
	Cg_SelectWeapon(-1);
}

/**
 * @brief Draws the HUD for the current frame.
 */
void Cg_DrawHud(const player_state_t *ps) {

	if (!cg_draw_hud->integer) {
		return;
	}

	if (!ps->stats[STAT_TIME]) { // intermission
		return;
	}

	Cg_DrawVitals(ps);

	Cg_DrawPowerups(ps);

	Cg_DrawHeldFlag(ps);

	Cg_DrawHeldTech(ps);

	Cg_DrawPickup(ps);

	Cg_DrawTeamBanner(ps);

	Cg_DrawFrags(ps);

	Cg_DrawDeaths(ps);

	Cg_DrawCaptures(ps);

	Cg_DrawSpectator(ps);

	Cg_DrawChase(ps);

	Cg_DrawVote(ps);

	Cg_DrawTime(ps);

	Cg_DrawReady(ps);

	Cg_DrawCrosshair(ps);

	Cg_DrawCenterPrint(ps);

	Cg_DrawTargetName(ps);

	Cg_DrawBlend(ps);

	Cg_DrawDamageInflicted(ps);

	Cg_DrawSelectWeapon(ps);
}

/**
 * @brief Clear HUD-related state.
 */
void Cg_ClearHud(void) {
	memset(&cg_hud_locals, 0, sizeof(cg_hud_locals));

	cg_hud_locals.weapon.tag = WEAPON_SELECT_OFF;
}

/**
 * @brief
 */
void Cg_LoadHudMedia(void) {
	cg_select_weapon_image = cgi.LoadImage("pics/w_select", IT_PIC);
	cg_pickup_blend_image = cgi.LoadImage("pics/bf_pickup", IT_PIC);
	cg_quad_blend_image = cgi.LoadImage("pics/bf_powerup_quad", IT_PIC);
	cg_damage_blend_image = cgi.LoadImage("pics/bf_damage", IT_PIC);
}

/**
 * @brief
 */
void Cg_InitHud(void) {
	cgi.Cmd("cg_weapon_next", Cg_Weapon_Next_f, CMD_CGAME, "Open the weapon bar to the next weapon. In chasecam, switches to next target.");
	cgi.Cmd("cg_weapon_previous", Cg_Weapon_Prev_f, CMD_CGAME, "Open the weapon bar to the previous weapon. In chasecam, switches to previous target.");

	cg_select_weapon_delay = cgi.Cvar("cg_select_weapon_delay", "250", CVAR_ARCHIVE, "The amount of time, in milliseconds, to wait between changing weapons in the scroll view. Clicking will override this value and switch immediately.");
	cg_select_weapon_interval = cgi.Cvar("cg_select_weapon_interval", "750", CVAR_ARCHIVE, "The amount of time, in milliseconds, to show the weapon bar after changing weapons.");
}
