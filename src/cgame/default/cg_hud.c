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

#define HUD_COLOR_STAT			color_white
#define HUD_COLOR_STAT_MED		color_yellow
#define HUD_COLOR_STAT_LOW		color_red

#define HUD_PIC_HEIGHT			64

#define HUD_HEALTH_MED			75
#define HUD_HEALTH_LOW			25

#define HUD_ARMOR_MED			50
#define HUD_ARMOR_LOW			25

#define HUD_POWERUP_LOW			5

#define CENTER_PRINT_LINES 8

/**
 * @brief The center print, parsed from SV_CMD_CENTER_PRINT.
 */
static struct {
	char lines[CENTER_PRINT_LINES][MAX_STRING_CHARS];
	int32_t num_lines;
	uint32_t time;
} cg_center_print;

/**
 * @brief The client's weapon cache, parsed from player state stat bits.
 */
typedef struct {
	int16_t icon_index;
	int16_t item_index;
} cg_hud_weapon_t;

static cg_hud_weapon_t cg_hud_weapons[MAX_STAT_BITS];

#define WEAPON_SELECT_OFF				-1

/**
 * @brief The HUD state, cleared when respawning or changing chase targets.
 */
static struct {

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
		uint32_t time, bar_time;
		int16_t bits;
		int16_t num;
		_Bool has[MAX_STAT_BITS];
	} weapon;

	int16_t chase_target;
} cg_hud_state;


static r_image_t *cg_pickup_blend_image;
static r_image_t *cg_quad_blend_image;
static r_image_t *cg_damage_blend_image;

static r_image_t *cg_select_weapon_image;

static cvar_t *cg_select_weapon_alpha;
static cvar_t *cg_select_weapon_delay;
static cvar_t *cg_select_weapon_fade;
static cvar_t *cg_select_weapon_interval;

/**
 * @brief Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(const r_pixel_t x, const r_pixel_t y, const int16_t icon, const color_t color) {

	const r_image_t *image = cgi.client->images[icon];
	if (!image) {
		cgi.Warn("Invalid icon: %d\n", icon);
		return;
	}

	cgi.Draw2DImage(x, y, image->width, image->height, image, color);
}

/**
 * @brief Draws the vital numeric and icon, flashing on low quantities.
 */
static void Cg_DrawVital(r_pixel_t x, r_pixel_t ch, const int16_t value, const int16_t icon, int16_t med, int16_t low) {
	r_pixel_t y = cgi.context->height - HUD_PIC_HEIGHT + (HUD_PIC_HEIGHT - ch) / 2;

	color_t color = HUD_COLOR_STAT;
	color_t pulse = color_white;

	if (value < low) {
		if (cg_draw_vitals_pulse->integer) {
			pulse.a = Clampf(sinf(cgi.client->unclamped_time / 250.f), 0.75f, 1.f);
		}
		color = HUD_COLOR_STAT_LOW;
	} else if (value < med) {
		color = HUD_COLOR_STAT_MED;
	}

	const char *string = va("%3d", value);

	cgi.Draw2DString(x, y, string, color);

	x += cgi.StringWidth(string) + 5;
	y = cgi.context->height - HUD_PIC_HEIGHT - 5;

	Cg_DrawIcon(x, y, icon, pulse);
}

/**
 * @brief Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(const player_state_t *ps) {
	r_pixel_t x, cw, ch, x_offset;

	if (!cg_draw_vitals->integer) {
		return;
	}

	cgi.BindFont("large", &cw, &ch);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const int16_t health = ps->stats[STAT_HEALTH];
		const int16_t health_icon = ps->stats[STAT_HEALTH_ICON];

		x = cgi.context->width * 0.5 - x_offset;

		Cg_DrawVital(x, ch, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if (cg_state.gameplay != GAME_INSTAGIB) {

		if (ps->stats[STAT_AMMO] > 0) {
			const int16_t ammo = ps->stats[STAT_AMMO];
			const int16_t ammo_low = ps->stats[STAT_AMMO_LOW];
			const int16_t ammo_icon = ps->stats[STAT_AMMO_ICON];

			x = cgi.context->width * 0.25 - x_offset;

			Cg_DrawVital(x, ch, ammo, ammo_icon, -1, ammo_low);
		}

		if (ps->stats[STAT_ARMOR] > 0) {
			const int16_t armor = ps->stats[STAT_ARMOR];
			const int16_t armor_icon = ps->stats[STAT_ARMOR_ICON];

			x = cgi.context->width * 0.75 - x_offset;

			Cg_DrawVital(x, ch, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
		}
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Draws the powerup and the time remaining
 */
static void Cg_DrawPowerup(r_pixel_t y, const int16_t value, const r_image_t *icon) {
	r_pixel_t x;

	color_t color = HUD_COLOR_STAT;

	if (value < HUD_POWERUP_LOW) {
		color = HUD_COLOR_STAT_LOW;
	}

	const char *string = va("%d", value);

	x = HUD_PIC_HEIGHT / 2;

	cgi.Draw2DImage(x, y, icon->width, icon->height, icon, color_white);

	x += HUD_PIC_HEIGHT;

	cgi.Draw2DString(x, y, string, color);
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

	y = cgi.context->height / 2;

	if (ps->stats[STAT_QUAD_TIME] > 0) {
		const int32_t timer = ps->stats[STAT_QUAD_TIME];
		Cg_DrawPowerup(y, timer, cgi.LoadImage("pics/i_quad", IT_PIC));
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

	const int16_t flag = ps->stats[STAT_CARRYING_FLAG];
	if (!flag) {
		return;
	}

	color_t pulse = color_white;
	pulse.a = Clampf(sinf(cgi.client->unclamped_time / 150.0), 0.75f, 1.f);

	x = HUD_PIC_HEIGHT / 2;
	y = cgi.context->height / 2 - HUD_PIC_HEIGHT * 2;

	const r_image_t *icon = cgi.LoadImage(va("pics/i_flag%d", flag),  IT_PIC);
	cgi.Draw2DImage(x, y, icon->width, icon->height, icon, pulse);
}

/**
 * @brief Draws the flag you are currently holding
 */
static void Cg_DrawHeldTech(const player_state_t *ps) {
	r_pixel_t x, y;

	if (!cg_draw_held_tech->integer) {
		return;
	}

	const int16_t tech = ps->stats[STAT_TECH_ICON];
	if (tech == -1) {
		return;
	}

	color_t pulse = color_white;
	pulse.a = Clampf(sinf(cgi.client->unclamped_time / 150.0), 0.75f, 1.f);

	x = 4;
	y = cgi.context->height / 2 - HUD_PIC_HEIGHT * 4;

	Cg_DrawIcon(x, y, tech, pulse);
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

		x = cgi.context->width - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = 0;

		Cg_DrawIcon(x, y, icon, color_white);

		x += HUD_PIC_HEIGHT;
		y += (HUD_PIC_HEIGHT - ch) / 2 + 2;

		cgi.Draw2DString(x, y, string, HUD_COLOR_STAT);
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

	x = cgi.context->width - cgi.StringWidth("Frags");
	y = HUD_PIC_HEIGHT + ch;

	cgi.Draw2DString(x, y, "Frags", color_green);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.context->width - 3 * cw;

	cgi.Draw2DString(x, y, va("%3d", frags), HUD_COLOR_STAT);

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

	x = cgi.context->width - cgi.StringWidth("Deaths");
	y = 2 * (HUD_PIC_HEIGHT + ch);

	cgi.Draw2DString(x, y, "Deaths", color_green);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.context->width - 3 * cw;

	cgi.Draw2DString(x, y, va("%3d", deaths), HUD_COLOR_STAT);

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

	if (!cg_state.ctf) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	x = cgi.context->width - cgi.StringWidth("Captures");
	y = 3 * (HUD_PIC_HEIGHT + ch);

	cgi.Draw2DString(x, y, "Captures", color_green);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.context->width - 3 * cw;

	cgi.Draw2DString(x, y, va("%3d", captures), HUD_COLOR_STAT);

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

	x = cgi.context->width - cgi.StringWidth("Spectating");
	y = HUD_PIC_HEIGHT;

	cgi.Draw2DString(x, y, "Spectating", color_green);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawChase(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_USER_INFO_VALUE * 2], *s;

	// if we've changed chase targets, reset the HUD
	if (ps->stats[STAT_CHASE] != cg_hud_state.chase_target) {
		memset(&cg_hud_state, 0, sizeof(cg_hud_state));
		cg_hud_state.chase_target = ps->stats[STAT_CHASE];
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

	x = cgi.context->width * 0.5 - cgi.StringWidth(string) / 2;
	y = cgi.context->height - HUD_PIC_HEIGHT - ch;

	cgi.Draw2DString(x, y, string, color_green);

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

	x = cgi.context->width - cgi.StringWidth(string);
	y = 3 * (HUD_PIC_HEIGHT + ch);

	if (cg_state.ctf) {
		y += HUD_PIC_HEIGHT + ch;
	}

	cgi.Draw2DString(x, y, string, color_white);

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

	x = cgi.context->width - cgi.StringWidth("Ready");
	y = 3 * (HUD_PIC_HEIGHT + ch);

	if (cg_state.ctf) {
		y += HUD_PIC_HEIGHT + ch;
	}

	y += ch;

	cgi.Draw2DString(x, y, "Ready", color_green);

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

	const color_t color = ColorHSVA(cg_state.teams[team].hue, 1.f, 1.f, .14f);

	x = 0;
	y = cgi.context->height - 64;

	cgi.Draw2DFill(x, y, cgi.context->width, 64, color);
}

/**
 * @brief
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {

	static struct {
		r_image_t *image;
		vec4_t color;
	} crosshair;

	r_pixel_t x, y, w, h;

	if (!cg_draw_crosshair->value) {
		return;
	}

	if (cgi.client->third_person) {
		return;
	}

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) {
		return; // spectating
	}

	if (!ps->stats[STAT_WEAPONS]) {
		return; // dead
	}

	if (cg_center_print.time > cgi.client->unclamped_time) {
		return;
	}

	if (cg_draw_crosshair->modified) {
		cg_draw_crosshair->modified = false;

		if (cg_draw_crosshair->value < 0) {
			cg_draw_crosshair->value = 1;
		}

		if (cg_draw_crosshair->value > 100) {
			cg_draw_crosshair->value = 100;
		}

		crosshair.image = cgi.LoadImage(va("pics/ch%d", cg_draw_crosshair->integer), IT_PIC);

		if (crosshair.image == NULL) {
			cgi.Print("Couldn't load pics/ch%d.\n", cg_draw_crosshair->integer);
			return;
		}
	}

	if (crosshair.image == NULL) { // not found
		return;
	}

	if (cg_draw_crosshair_color->modified) { // crosshair color
		cg_draw_crosshair_color->modified = false;

		color_t color = color_white;

		if (g_strcmp0(cg_draw_crosshair_color->string, "default")) {
			if (!Color_Parse(cg_draw_crosshair_color->string, &color)) {
				color = color_white;
			}
		}

		crosshair.color = Color_Vec4(color);
	}

	if (cg_draw_crosshair_health->integer == CROSSHAIR_HEALTH_RED_WHITE) {
		float health_frac = Clampf(ps->stats[STAT_HEALTH] / 100.0, 0.0, 1.0);

		crosshair.color.x = 1.0;
		crosshair.color.y = health_frac;
		crosshair.color.z = health_frac;
	} else if (cg_draw_crosshair_health->integer == CROSSHAIR_HEALTH_RED_WHITE_GREEN) {
		float health_frac = Clampf(ps->stats[STAT_HEALTH] / 100.0, 0.0, 1.0);
		float health_over = Clampf(((ps->stats[STAT_HEALTH] - 100) / 100.0), 0.0, 1.0);

		if (ps->stats[STAT_HEALTH] <= 100) {
			crosshair.color.x = 1.0;
			crosshair.color.y = health_frac;
			crosshair.color.z = health_frac;
		} else {
			crosshair.color.x = 1.0 - health_over;
			crosshair.color.y = 1.0;
			crosshair.color.z = 1.0 - health_over;
		}
	} else if (cg_draw_crosshair_health->integer == CROSSHAIR_HEALTH_RED_YELLOW_WHITE) {
		float health_frac_low = Clampf((ps->stats[STAT_HEALTH] - 15) / 50.0, 0.0, 1.0);
		float health_frac_medium = Clampf((ps->stats[STAT_HEALTH] - 65) / 35.0, 0.0, 1.0);

		if (ps->stats[STAT_HEALTH] <= 20) {
			crosshair.color.x = 1.0;
			crosshair.color.y = 0.0;
			crosshair.color.z = 0.0;
		} else if (ps->stats[STAT_HEALTH] <= 70) {
			crosshair.color.x = 1.0;
			crosshair.color.y = health_frac_low;
			crosshair.color.z = 0.0;
		} else {
			crosshair.color.x = 1.0;
			crosshair.color.y = 1.0;
			crosshair.color.z = health_frac_medium;
		}
	} else if (cg_draw_crosshair_health->integer == CROSSHAIR_HEALTH_RED_YELLOW_WHITE_GREEN) {
		float health_frac_low = Clampf((ps->stats[STAT_HEALTH] - 15) / 50.0, 0.0, 1.0);
		float health_frac_medium = Clampf((ps->stats[STAT_HEALTH] - 65) / 35.0, 0.0, 1.0);
		float health_over = Clampf(((ps->stats[STAT_HEALTH] - 100) / 100.0), 0.0, 1.0);

		if (ps->stats[STAT_HEALTH] <= 20) {
			crosshair.color.x = 1.0;
			crosshair.color.y = 0.0;
			crosshair.color.z = 0.0;
		} else if (ps->stats[STAT_HEALTH] <= 70) {
			crosshair.color.x = 1.0;
			crosshair.color.y = health_frac_low;
			crosshair.color.z = 0.0;
		} else if (ps->stats[STAT_HEALTH] <= 100) {
			crosshair.color.x = 1.0;
			crosshair.color.y = 1.0;
			crosshair.color.z = health_frac_medium;
		} else {
			crosshair.color.x = 1.0 - health_over;
			crosshair.color.y = 1.0;
			crosshair.color.z = 1.0 - health_over;
		}
	} else if (cg_draw_crosshair_health->integer == CROSSHAIR_HEALTH_WHITE_GREEN) {
		float health_over = (1.0 - Clampf(((ps->stats[STAT_HEALTH] - 100) / 100.0), 0.0, 1.0));

		if (ps->stats[STAT_HEALTH] <= 100) {
			crosshair.color.x = 1.0;
			crosshair.color.y = 1.0;
			crosshair.color.z = 1.0;
		} else {
			crosshair.color.x = 1.0 - health_over;
			crosshair.color.y = 1.0;
			crosshair.color.z = 1.0 - health_over;
		}
	}

	float scale = cg_draw_crosshair_scale->value * CROSSHAIR_SCALE;

	vec4_t color = crosshair.color;

	// pulse the crosshair size and alpha based on pickups
	if (cg_draw_crosshair_pulse->value) {

		const int16_t p = ps->stats[STAT_PICKUP_ICON];
		if (p != -1 && (p != cg_hud_state.pulse.pickup)) {
			cg_hud_state.pulse.time = cgi.client->unclamped_time;
		}

		cg_hud_state.pulse.pickup = p;

		const uint32_t delta = cgi.client->unclamped_time - cg_hud_state.pulse.time;
		if (delta < 300) {
			const float frac = delta / 300.f;
			scale += sinf(frac * M_PI) * CROSSHAIR_SCALE;
			color.w += sinf((frac - 1.f) * M_PI) * CROSSHAIR_PULSE_ALPHA;
		}
	}

	color.w *= cg_draw_crosshair_alpha->value;

	// calculate width and height based on crosshair image and scale
	w = crosshair.image->width * scale;
	h = crosshair.image->height * scale;

	x = (cgi.context->width - w) / 2.0;
	y = (cgi.context->height - h) / 2.0;

	cgi.Draw2DImage(x, y, w, h, crosshair.image, Color4fv(color));
}

/**
 * @brief
 */
void Cg_ParseCenterPrint(void) {
	char *c, *out, *line;

	memset(&cg_center_print, 0, sizeof(cg_center_print));

	c = cgi.ReadString();

	line = cg_center_print.lines[0];
	out = line;

	while (*c && cg_center_print.num_lines < CENTER_PRINT_LINES - 1) {

		if (*c == '\n') {
			line += MAX_STRING_CHARS;
			out = line;
			cg_center_print.num_lines++;
			c++;
			continue;
		}

		*out++ = *c++;
	}

	cg_center_print.num_lines++;
	cg_center_print.time = cgi.client->unclamped_time + 3000;
}

/**
 * @brief
 */
static void Cg_DrawCenterPrint(const player_state_t *ps) {
	r_pixel_t cw, ch, x, y;
	char *line = cg_center_print.lines[0];

	if (ps->stats[STAT_SCORES]) {
		return;
	}

	if (cg_center_print.time < cgi.client->unclamped_time) {
		return;
	}

	cgi.BindFont(NULL, &cw, &ch);

	y = (cgi.context->height - cg_center_print.num_lines * ch) / 2;

	while (*line) {
		x = (cgi.context->width - cgi.StringWidth(line)) / 2;

		cgi.Draw2DString(x, y, line, color_white);
		line += MAX_STRING_CHARS;
		y += ch;
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief Perform composition of the dst/src blends.
 */
static void Cg_AddBlend(color_t *blend, const color_t input) {

	if (input.a <= 0.0) {
		return;
	}

	color_t out = *blend;

	out.a = input.a + out.a * (1.0 - input.a);

	for (int32_t i = 0; i < 3; i++) {
		out.rgba[i] = ((input.rgba[i] * input.a) + ((out.rgba[i] * out.a) * (1.0 - input.a))) / out.a;
	}

	*blend = out;
}

/**
 * @brief Calculate the alpha factor for the specified blend components.
 * @param blend_start_time The start of the blend, in unclamped time.
 * @param blend_decay_time The length of the blend in milliseconds.
 * @param blend_alpha The base alpha value.
 */
static float Cg_CalculateBlendAlpha(const uint32_t blend_start_time, const uint32_t blend_decay_time,
                                    const float blend_alpha) {

	if ((cgi.client->unclamped_time - blend_start_time) <= blend_decay_time) {
		const float time_factor = (float) (cgi.client->unclamped_time - blend_start_time) / blend_decay_time;
		const float alpha = cg_draw_blend->value * (blend_alpha - (time_factor * blend_alpha));

		return alpha;
	}

	return 0.0;
}

/**
 * @brief Draw a blend flash image with a specified alpha.
 * @param icon The picture to use
 * @param alpha The alpha of the blend
 */
static void Cg_DrawBlendFlashImage(const r_image_t *image, const float alpha) {

	if (alpha <= 0.0) {
		return;
	}

	const color_t color = Color4f(1.0, 1.0, 1.0, alpha);
	cgi.Draw2DImage(0, 0, cgi.context->width, cgi.context->height, image, color);
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

	color_t blend = color_transparent;
	
	// start with base blend based on view origin conents

	const int32_t contents = cgi.view->contents;

	if ((contents & CONTENTS_MASK_LIQUID) && cg_draw_blend_liquid->value) {
		color_t color;
		if (contents & CONTENTS_LAVA) {
			color = Color4f(.8f, .4f, .1f, 1.f);
		} else if (contents & CONTENTS_SLIME) {
			color = Color4f(.4f, .7f, .2f, 1.f);
		} else {
			color = Color4f(.4f, .5f, .6f, 1.f);
		}

		color.a = Clampf(cg_draw_blend_liquid->value * 0.4, 0.f, 0.4f);

		Cg_AddBlend(&blend, color);
	}

	// pickups

	const int16_t p = ps->stats[STAT_PICKUP_ICON] & ~STAT_TOGGLE_BIT;

	if (p > -1 && (p != cg_hud_state.blend.pickup)) { // don't flash on same item
		cg_hud_state.blend.pickup_time = cgi.client->unclamped_time;
	}

	cg_hud_state.blend.pickup = p;

	if (cg_hud_state.blend.pickup_time && cg_draw_blend_pickup->value) {
		Cg_DrawBlendFlashImage(cg_pickup_blend_image,
			Cg_CalculateBlendAlpha(cg_hud_state.blend.pickup_time, CG_PICKUP_BLEND_TIME, cg_draw_blend_pickup->value));
	}

	// quad damage powerup

	if (ps->stats[STAT_QUAD_TIME] > 0 && cg_draw_blend_powerup->value) {
		Cg_DrawBlendFlashImage(cg_quad_blend_image,
			fabsf(sinf(Radians(cgi.client->unclamped_time * 0.2))) * cg_draw_blend_powerup->value);
	}

	// taken damage

	const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

	if (d) {
		cg_hud_state.blend.damage_time = cgi.client->unclamped_time;
	}

	if (cg_hud_state.blend.damage_time && cg_draw_blend_damage->value) {
		Cg_DrawBlendFlashImage(cg_damage_blend_image,
			Cg_CalculateBlendAlpha(cg_hud_state.blend.damage_time, CG_DAMAGE_BLEND_TIME, cg_draw_blend_damage->value));
	}

	// if we have a blend, draw it

	if (blend.a > 0.0) {
		cgi.Draw2DFill(0, 0, cgi.context->width, cgi.context->height, blend);
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
		if (cgi.client->unclamped_time - cg_hud_state.damage.hit_sound_time > 50) {
			cg_hud_state.damage.hit_sound_time = cgi.client->unclamped_time;

			Cg_AddSample(cgi.stage, &(const s_play_sample_t) {
				.sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0],
				.entity = Cg_Self()->current.number
			});
		}
	}
}

/**
 * @brief
 */
void Cg_ParseWeaponInfo(const char *s) {

	Cg_Debug("Received weapon info from server: %s\n", s);

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
	if (cg_hud_state.weapon.tag == WEAPON_SELECT_OFF) {
		cg_hud_state.weapon.tag = Cg_ActiveWeapon(ps);
		return;
	}

	// see if we have this weapon
	if (cg_hud_state.weapon.has[cg_hud_state.weapon.tag]) {
		return; // got it
	}

	// nope, so pick the closest one we have
	for (int32_t i = 2; i < MAX_STAT_BITS * 2; i++) {
		int32_t offset = (int32_t) (((i & 1) ? -i : i) / 2);
		int32_t id = cg_hud_state.weapon.tag + offset;

		if (id < 0 || id >= MAX_STAT_BITS) {
			continue;
		}

		if (cg_hud_state.weapon.has[id]) {
			cg_hud_state.weapon.tag = id;
			return;
		}
	}

	// should never happen
	cg_hud_state.weapon.tag = WEAPON_SELECT_OFF;
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

	for (int32_t i = 0; i < MAX_STAT_BITS; i++) {

		cg_hud_state.weapon.tag += dir;

		if (cg_hud_state.weapon.tag < 0) {
			cg_hud_state.weapon.tag = MAX_STAT_BITS - 1;
		} else if (cg_hud_state.weapon.tag >= MAX_STAT_BITS) {
			cg_hud_state.weapon.tag = 0;
		}

		if (cg_hud_state.weapon.has[cg_hud_state.weapon.tag]) {
			cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_delay->integer;
			cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
			return;
		}
	}

	// should never happen
	cg_hud_state.weapon.tag = WEAPON_SELECT_OFF;
}

/**
 * @brief
 */
_Bool Cg_AttemptSelectWeapon(const player_state_t *ps) {

	cg_hud_state.weapon.time = 0;

	if (!ps->stats[STAT_SPECTATOR] &&
		cg_hud_state.weapon.tag != -1) {

		if (cg_hud_state.weapon.tag != Cg_ActiveWeapon(ps)) {
			const char *name = cgi.client->config_strings[CS_ITEMS + cg_hud_weapons[cg_hud_state.weapon.tag].item_index];
			cgi.Cbuf(va("use %s\n", name));

			cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
			cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;

			return true;
		}

		cg_hud_state.weapon.tag = -1;
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
		cg_hud_state.weapon.tag = -1;
		cg_hud_state.weapon.time = 0;
		cg_hud_state.weapon.bar_time = 0;
		cg_hud_state.weapon.used_tag = 0;
		return;
	}

	// see if we have any weapons at all
	if (cg_hud_state.weapon.bits != ps->stats[STAT_WEAPONS])
	{
		cg_hud_state.weapon.bits = ps->stats[STAT_WEAPONS];
		cg_hud_state.weapon.num = 0;

		for (int32_t i = 0; i < MAX_STAT_BITS; i++) {
			cg_hud_state.weapon.has[i] = !!(cg_hud_state.weapon.bits & (1 << i));

			if (cg_hud_state.weapon.has[i])
				cg_hud_state.weapon.num++;
		}
	}

	if (!cg_hud_state.weapon.num) {
		cg_hud_state.weapon.tag = -1;
		cg_hud_state.weapon.time = 0;
		cg_hud_state.weapon.bar_time = 0;
		cg_hud_state.weapon.used_tag = 0;
		return;
	}

	const int16_t switching = ((ps->stats[STAT_WEAPON_TAG] >> 8) & 0xFF);

	if (cg_hud_state.weapon.used_tag != switching) {
		cg_hud_state.weapon.used_tag = switching;

		if (cg_hud_state.weapon.used_tag && !ps->stats[STAT_SPECTATOR]) {

			// we changed weapons without using scrolly, show it for a bit
			cg_hud_state.weapon.tag = cg_hud_state.weapon.used_tag - 1;
			cg_hud_state.weapon.time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
			cg_hud_state.weapon.bar_time = cgi.client->unclamped_time + cg_select_weapon_interval->integer;
		}
	}

	// not changing or ran out of time
	if (cg_hud_state.weapon.time <= cgi.client->unclamped_time) {
		Cg_AttemptSelectWeapon(ps);

		if (cg_hud_state.weapon.time <= cgi.client->unclamped_time) {
			return;
		}
	}

	// figure out weapon.tag
	Cg_ValidateSelectedWeapon(ps);

	r_pixel_t x = ((cgi.context->width / 2) - ((cg_hud_state.weapon.num * HUD_PIC_HEIGHT) / 2));
	r_pixel_t y = cgi.context->height - (HUD_PIC_HEIGHT * 2.0) - 16;

	// draw the weapons inventory bar
	r_pixel_t ch;
	cgi.BindFont("medium", NULL, &ch);

	if (cg_select_weapon_fade->modified || cg_select_weapon_interval->modified) {
		cg_select_weapon_fade->modified = false;

		cg_select_weapon_fade->value = Clampf(cg_select_weapon_fade->value, 0.f, cg_select_weapon_interval->value);
	}

	const int32_t delta = cg_hud_state.weapon.bar_time - cgi.client->unclamped_time;
	const float alpha = Minf(delta / (float) cg_select_weapon_fade->integer, 1.0);

	const color_t color_selection = Color4f(1.f, 1.f, 1.f, alpha);
	const color_t color = Color4f(1.f, 1.f, 1.f, alpha * cg_select_weapon_alpha->value);

	for (int32_t i = 0; i < MAX_STAT_BITS; i++) {

		if (!cg_hud_state.weapon.has[i]) {
			continue;
		}

		const color_t c = (i == cg_hud_state.weapon.tag) ? color_selection : color;

		Cg_DrawIcon(x, y, cg_hud_weapons[i].icon_index, c);

		if (i == cg_hud_state.weapon.tag) {
			const char *name = cgi.client->config_strings[CS_ITEMS + cg_hud_weapons[i].item_index];
			cgi.Draw2DString(((cgi.context->width / 2) - (cgi.StringWidth(name) / 2)), y - ch, name, HUD_COLOR_STAT);
			cgi.Draw2DImage(x,
						  y,
						  cg_select_weapon_image->width,
						  cg_select_weapon_image->height,
						  cg_select_weapon_image,
						  color_white);
		}

		x += HUD_PIC_HEIGHT + 4;
	}

	cgi.BindFont(NULL, NULL, NULL);
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

	vec3_t pos = Vec3_Fmaf(cgi.view->origin, MAX_WORLD_DIST, cgi.view->forward);

	const cm_trace_t tr = cgi.Trace(cgi.view->origin, pos, Box3_Zero(), 0, CONTENTS_MASK_CLIP_PROJECTILE);
	if (tr.fraction < 1.0) {

		const cl_entity_t *ent = &cgi.client->entities[(ptrdiff_t) tr.ent];
		if (ent->current.model1 == MODEL_CLIENT) {

			const cg_client_info_t *client = &cg_state.clients[ent->current.client];

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
		const r_pixel_t x = cgi.context->width / 2 - w / 2;
		const r_pixel_t y = cgi.context->height - 192 - ch;

		cgi.Draw2DString(x, y, name, color_green);
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
	memset(&cg_hud_state, 0, sizeof(cg_hud_state));

	cg_hud_state.weapon.tag = WEAPON_SELECT_OFF;
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
	cgi.AddCmd("cg_weapon_next", Cg_Weapon_Next_f, CMD_CGAME,
			   "Open the weapon bar to the next weapon. In chasecam, switches to next target.");
	cgi.AddCmd("cg_weapon_previous", Cg_Weapon_Prev_f, CMD_CGAME,
			   "Open the weapon bar to the previous weapon. In chasecam, switches to previous target.");

	cg_select_weapon_alpha = cgi.AddCvar("cg_select_weapon_alpha", "0.5", CVAR_ARCHIVE,
										 "The opacity of unselected weapons in the weapon bar.");
	cg_select_weapon_delay = cgi.AddCvar("cg_select_weapon_delay", "250", CVAR_ARCHIVE,
										 "The amount of time, in milliseconds, to wait between changing weapons in the scroll view.");
	cg_select_weapon_fade = cgi.AddCvar("cg_select_weapon_fade", "200", CVAR_ARCHIVE,
										 "The amount of time, in milliseconds, for the weapon bar to fade in or out.");
	cg_select_weapon_interval = cgi.AddCvar("cg_select_weapon_interval", "750", CVAR_ARCHIVE,
											"The amount of time, in milliseconds, to show the weapon bar after changing weapons.");
}
