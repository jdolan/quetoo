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
#define HUD_COLOR_STAT_STRING	CON_COLOR_DEFAULT
#define COLOR_SCORES_HEADER		CON_COLOR_ALT

#define CROSSHAIR_COLOR_RED		242
#define CROSSHAIR_COLOR_GREEN	209
#define CROSSHAIR_COLOR_YELLOW	219
#define CROSSHAIR_COLOR_ORANGE  225
#define CROSSHAIR_COLOR_DEFAULT	15

#define HUD_PIC_HEIGHT			64

#define HUD_HEALTH_MED			40
#define HUD_HEALTH_LOW			20

#define HUD_ARMOR_MED			40
#define HUD_ARMOR_LOW			20

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
			pulse[3] = sin(cgi.client->systime / 250.0) + 0.75;
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

	if (!cg_draw_vitals->integer)
		return;

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const int16_t health = ps->stats[STAT_HEALTH];
		const int16_t health_icon = ps->stats[STAT_HEALTH_ICON];

		x = cgi.view->viewport.w * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

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

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawPickup(const player_state_t *ps) {
	r_pixel_t x, y, cw, ch;

	if (!cg_draw_pickup->integer)
		return;

	cgi.BindFont(NULL, &cw, &ch);

	if (ps->stats[STAT_PICKUP_ICON] > 0) {
		const int16_t icon = ps->stats[(STAT_PICKUP_ICON & ~STAT_TOGGLE_BIT)];
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

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return;

	if (!cg_draw_frags->integer)
		return;

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

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return;

	if (!cg_draw_deaths->integer)
		return;

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

	if (!cg_draw_captures->integer)
		return;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return;

	if (atoi(cgi.ConfigString(CS_CTF)) < 1)
		return;

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

	if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE])
		return;

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

	if (!ps->stats[STAT_CHASE])
		return;

	const int32_t c = ps->stats[STAT_CHASE];

	if (c - CS_CLIENTS >= MAX_CLIENTS) {
		cgi.Warn("Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	g_snprintf(string, sizeof(string), "Chasing ^7%s", cgi.ConfigString(c));

	if ((s = strchr(string, '\\')))
		*s = '\0';

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

	if (!cg_draw_vote->integer)
		return;

	if (!ps->stats[STAT_VOTE])
		return;

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

	if (!ps->stats[STAT_TIME])
		return;

	if (!cg_draw_time->integer)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth(string);
	y = cgi.view->viewport.y + 3 * (HUD_PIC_HEIGHT + ch);

	if (atoi(cgi.ConfigString(CS_CTF)) > 0)
		y += HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, string, CON_COLOR_DEFAULT);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawReady(const player_state_t *ps) {
	r_pixel_t x, y, ch;

	if (!ps->stats[STAT_READY])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->viewport.x + cgi.view->viewport.w - cgi.StringWidth("Ready");
	y = cgi.view->viewport.y + HUD_PIC_HEIGHT * 2 + 2 * ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @brief
 */
static void Cg_DrawTeam(const player_state_t *ps) {
	const int16_t team = ps->stats[STAT_TEAM];
	r_pixel_t x, y;
	int32_t color;

	if (!team)
		return;

	if (!cg_draw_teambar->integer)
		return;

	if (team == CS_TEAM_GOOD)
		color = 243;
	else if (team == CS_TEAM_EVIL)
		color = 242;
	else {
		cgi.Warn("Unknown team: %d\n", team);
		return;
	}

	x = cgi.view->viewport.x;
	y = cgi.view->viewport.y + cgi.view->viewport.h - 64;

	cgi.DrawFill(x, y, cgi.view->viewport.w, 64, color, 0.15);
}

/**
 * @brief
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {
	r_pixel_t x, y;
	int32_t color;

	if (!cg_draw_crosshair->value)
		return;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return; // spectating

	if (center_print.time > cgi.client->systime)
		return;

	if (cg_draw_crosshair->modified) { // crosshair image
		cg_draw_crosshair->modified = false;

		if (cg_draw_crosshair->value < 0)
			cg_draw_crosshair->value = 1;

		if (cg_draw_crosshair->value > 100)
			cg_draw_crosshair->value = 100;

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

		const char *c = cg_draw_crosshair_color->string;
		if (!g_ascii_strcasecmp(c, "red")) {
			color = CROSSHAIR_COLOR_RED;
		} else if (!g_ascii_strcasecmp(c, "green")) {
			color = CROSSHAIR_COLOR_GREEN;
		} else if (!g_ascii_strcasecmp(c, "yellow")) {
			color = CROSSHAIR_COLOR_YELLOW;
		} else if (!g_ascii_strcasecmp(c, "orange")) {
			color = CROSSHAIR_COLOR_ORANGE;
		} else {
			color = CROSSHAIR_COLOR_DEFAULT;
		}

		cgi.ColorFromPalette(color, crosshair.color);
	}

	vec_t scale = cg_draw_crosshair_scale->value * (cgi.context->high_dpi ? 2.0 : 1.0);
	crosshair.color[3] = 1.0;

	// pulse the crosshair size and alpha based on pickups
	if (cg_draw_crosshair_pulse->value) {
		static uint32_t last_pulse_time;
		static int16_t pickup;

		// determine if we've picked up an item
		const int16_t p = ps->stats[STAT_PICKUP_ICON];

		if (p && (p != pickup)) {
			last_pulse_time = cgi.client->systime;
		}

		pickup = p;

		const vec_t delta = 1.0 - ((cgi.client->systime - last_pulse_time) / 500.0);

		if (delta > 0.0) {
			scale += cg_draw_crosshair_pulse->value * 0.5 * delta;
			crosshair.color[3] -= 0.5 * delta;
		}
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
	center_print.time = cgi.client->systime + 3000;
}

/**
 * @brief
 */
static void Cg_DrawCenterPrint(const player_state_t *ps) {
	r_pixel_t cw, ch, x, y;
	char *line = center_print.lines[0];

	if (ps->stats[STAT_SCORES])
		return;

	if (center_print.time < cgi.client->systime)
		return;

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
 * @brief Draw a full-screen blend effect based on world interaction.
 */
static void Cg_DrawBlend(const player_state_t *ps) {
	static int16_t pickup;
	static uint32_t last_blend_time;
	static int32_t color;
	static vec_t alpha;

	if (!cg_draw_blend->value)
		return;

	if (last_blend_time > cgi.client->systime)
		last_blend_time = 0;

	// determine if we've picked up an item
	const int16_t p = ps->stats[STAT_PICKUP_ICON];

	if (p && (p != pickup)) {
		last_blend_time = cgi.client->systime;
		color = 215;
		alpha = 0.3;
	}
	pickup = p;

	// or taken damage
	const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

	if (d) {
		last_blend_time = cgi.client->systime;
		color = 240;
		alpha = 0.3;
	}

	// determine the current blend color based on the above events
	vec_t t = (vec_t) (cgi.client->systime - last_blend_time) / 500.0;
	vec_t al = cg_draw_blend->value * (alpha - (t * alpha));

	if (al < 0.0 || al > 1.0)
		al = 0.0;

	// and finally, determine supplementary blend based on view origin conents
	const int32_t contents = cgi.view->contents;

	if (al < 0.3 * cg_draw_blend->value && (contents & MASK_LIQUID)) {
		if (al < 0.15 * cg_draw_blend->value) { // don't override damage or pickup blend
			if (contents & CONTENTS_LAVA)
				color = 71;
			else if (contents & CONTENTS_SLIME)
				color = 201;
			else
				color = 114;
		}
		al = 0.3 * cg_draw_blend->value;
	}

	// if we have a blend, draw it
	if (al > 0.0) {
		cgi.DrawFill(cgi.view->viewport.x, cgi.view->viewport.y,
					 cgi.view->viewport.w, cgi.view->viewport.h, color, al);
	}
}

/**
 * @brief Plays the hit sound if the player inflicted damage this frame.
 */
static void Cg_DrawDamageInflicted(const player_state_t *ps) {

	const int16_t dmg = ps->stats[STAT_DAMAGE_INFLICT];
	if (dmg) {
		static uint32_t last_damage_time;

		// wrap timer around level changes
		if (last_damage_time > cgi.client->systime) {
			last_damage_time = 0;
		}

		// play the hit sound
		if (cgi.client->systime - last_damage_time > 50) {
			last_damage_time = cgi.client->systime;
			
			cgi.AddSample(&(const s_play_sample_t) {
				.sample = dmg >= 25 ? cg_sample_hits[1] : cg_sample_hits[0]
			});
		}
	}
}

/**
 * @brief Draws the HUD for the current frame.
 */
void Cg_DrawHud(const player_state_t *ps) {

	if (!cg_draw_hud->integer)
		return;

	if (!ps->stats[STAT_TIME]) // intermission
		return;

	Cg_DrawVitals(ps);

	Cg_DrawPickup(ps);

	Cg_DrawTeam(ps);

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

	Cg_DrawBlend(ps);

	Cg_DrawDamageInflicted(ps);
}
