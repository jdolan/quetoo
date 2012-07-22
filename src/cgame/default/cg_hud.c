/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#define COLOR_HUD_STAT			CON_COLOR_DEFAULT
#define COLOR_HUD_STAT_MED		CON_COLOR_YELLOW
#define COLOR_HUD_STAT_LOW		CON_COLOR_RED
#define COLOR_HUD_STAT_STRING	CON_COLOR_DEFAULT
#define COLOR_SCORES_HEADER		CON_COLOR_ALT

#define HUD_PIC_HEIGHT			64

#define HUD_HEALTH_MED			40
#define HUD_HEALTH_LOW			20

#define HUD_ARMOR_MED			40
#define HUD_ARMOR_LOW			20

typedef struct cg_crosshair_s {
	char name[16];
	r_image_t *image;
	byte color[4];
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
 * Cg_DrawIcon
 *
 * Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(const r_pixel_t x, const r_pixel_t y, const float scale,
		const uint16_t icon) {

	if (icon >= MAX_IMAGES) {
		cgi.Warn("Cg_DrawIcon: Invalid icon: %d\n", icon);
		return;
	}

	cgi.DrawPic(x, y, scale, cgi.ConfigString(CS_IMAGES + icon));
}

/**
 * Cg_DrawVital
 *
 * Draws the vital numeric and icon, flashing on low quantities.
 */
static void Cg_DrawVital(r_pixel_t x, const int16_t value, const int16_t icon, int16_t med, int16_t low) {
	vec4_t flashColor = {1.0f, 1.0f, 1.0f, 1.0f};
	int32_t color = COLOR_HUD_STAT;
	r_pixel_t y = cgi.view->y + cgi.view->height - HUD_PIC_HEIGHT + 4;


	if (value < low) {

		if (cg_draw_vitals_pulse->integer)
			flashColor[3] = sin(cgi.client->time / 250.0) + 0.75f;

		color = COLOR_HUD_STAT_LOW;
	} else if (value < med) {
		color = COLOR_HUD_STAT_MED;
	}

	const char *string = va("%3d", value);

	x += cgi.view->x;
	cgi.DrawString(x, y, string, color);

	x += cgi.StringWidth(string);
	y = cgi.view->y + cgi.view->height - HUD_PIC_HEIGHT;

	cgi.Colorf(flashColor);
	Cg_DrawIcon(x, y, 1.0, icon);
	cgi.Colorf(NULL);
}

/*
 * Cg_DrawVitals
 *
 * Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(const player_state_t *ps) {
	r_pixel_t x, cw, x_offset;

	if(!cg_draw_vitals->integer)
		return;

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const int16_t health = ps->stats[STAT_HEALTH];
		const int16_t health_icon = ps->stats[STAT_HEALTH_ICON];

		x = cgi.view->width * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if (ps->stats[STAT_AMMO] > 0) {
		const int16_t ammo = ps->stats[STAT_AMMO];
		const int16_t ammo_low = ps->stats[STAT_AMMO_LOW];
		const int16_t ammo_icon = ps->stats[STAT_AMMO_ICON];

		x = cgi.view->width * 0.5 - x_offset;

		Cg_DrawVital(x, ammo, ammo_icon, -1, ammo_low);
	}

	if (ps->stats[STAT_ARMOR] > 0) {
		const int16_t armor = ps->stats[STAT_ARMOR];
		const int16_t armor_icon = ps->stats[STAT_ARMOR_ICON];

		x = cgi.view->width * 0.75 - x_offset;

		Cg_DrawVital(x, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawPickup
 */
static void Cg_DrawPickup(const player_state_t *ps) {
	r_pixel_t x, y, cw, ch;

	if(!cg_draw_pickup->integer)
		return;

	cgi.BindFont(NULL, &cw, &ch);

	if (ps->stats[STAT_PICKUP_ICON] > 0) {
		const int16_t icon = ps->stats[(STAT_PICKUP_ICON & ~STAT_TOGGLE_BIT)];
		const int16_t pickup = ps->stats[STAT_PICKUP_STRING];

		const char *string = cgi.ConfigString(pickup);

		x = cgi.view->x + cgi.view->width - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = cgi.view->y;

		Cg_DrawIcon(x, y, 1.0, icon);

		x += HUD_PIC_HEIGHT;
		y += ch / 2;

		cgi.DrawString(x, y, string, COLOR_HUD_STAT);
	}
}

/*
 * Cg_DrawFrags
 */
static void Cg_DrawFrags(const player_state_t *ps) {
	const int16_t frags = ps->stats[STAT_FRAGS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return;

	if (!cg_draw_frags->integer)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->x + cgi.view->width - cgi.StringWidth("Frags");
	y = cgi.view->y + HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Frags", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->x + cgi.view->width - 3 * cw;

	cgi.DrawString(x, y, va("%3d", frags), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawCaptures
 */
static void Cg_DrawCaptures(const player_state_t *ps) {
	const int16_t captures = ps->stats[STAT_CAPTURES];
	r_pixel_t x, y, cw, ch;

	if(!cg_draw_captures->integer)
		return;

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return;

	if (atoi(cgi.ConfigString(CS_CTF)) < 1)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->x + cgi.view->width - cgi.StringWidth("Captures");
	y = cgi.view->y + 2 * HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, "Captures", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = cgi.view->x + cgi.view->width - 3 * cw;

	cgi.DrawString(x, y, va("%3d", captures), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawSpectator
 */
static void Cg_DrawSpectator(const player_state_t *ps) {
	r_pixel_t x, y, cw;

	if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE])
		return;

	cgi.BindFont("small", &cw, NULL);

	x = cgi.view->width - cgi.StringWidth("Spectating");
	y = cgi.view->y + HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Spectating", CON_COLOR_GREEN);
}

/*
 * Cg_DrawChase
 */
static void Cg_DrawChase(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_USER_INFO_VALUE * 2], *s;

	if (!ps->stats[STAT_CHASE])
		return;

	const int32_t c = ps->stats[STAT_CHASE];

	if (c - CS_CLIENTS >= MAX_CLIENTS) {
		cgi.Warn("Cg_DrawChase: Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Chasing ^7%s", cgi.ConfigString(c));

	if ((s = strchr(string, '\\')))
		*s = '\0';

	x = cgi.view->x + cgi.view->width * 0.5 - cgi.StringWidth(string) / 2;
	y = cgi.view->y + cgi.view->height - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawVote
 */
static void Cg_DrawVote(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_STRING_CHARS];

	if(!cg_draw_vote->integer)
		return;

	if (!ps->stats[STAT_VOTE])
		return;

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Vote: ^7%s", cgi.ConfigString(ps->stats[STAT_VOTE]));

	x = cgi.view->x;
	y = cgi.view->y + cgi.view->height - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawTime
 */
static void Cg_DrawTime(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	const char *string = cgi.ConfigString(CS_TIME);

	if (!ps->stats[STAT_TIME])
		return;

	if (!cg_draw_time->integer)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->x + cgi.view->width - cgi.StringWidth(string);
	y = cgi.view->y + HUD_PIC_HEIGHT * 2 + ch;

	if (atoi(cgi.ConfigString(CS_CTF)) > 0)
		y += HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, string, CON_COLOR_DEFAULT);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawReady
 */
static void Cg_DrawReady(const player_state_t *ps) {
	r_pixel_t x, y, ch;

	if (!ps->stats[STAT_READY])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = cgi.view->x + cgi.view->width - cgi.StringWidth("Ready");
	y = cgi.view->y + HUD_PIC_HEIGHT * 2 + 2 * ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawTeamBanner
 */
static void Cg_DrawTeam(const player_state_t *ps) {
	const int16_t team = ps->stats[STAT_TEAM];
	r_pixel_t x, y;
	int32_t color;

	if (!team)
		return;

	if(!cg_draw_teambar->integer)
		return;

	if (team == CS_TEAM_GOOD)
		color = 243;
	else if (team == CS_TEAM_EVIL)
		color = 242;
	else {
		cgi.Warn("Cg_DrawTeamBanner: unknown team: %d.\n", team);
		return;
	}

	x = cgi.view->x;
	y = cgi.view->y + cgi.view->height - 64;

	cgi.DrawFill(x, y, cgi.view->width, 64, color, 0.15);
}

/*
 * Cg_DrawCrosshair
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {
	r_pixel_t x, y;
	int32_t color;

	if (!cg_draw_crosshair->value)
		return;

	if (ps->stats[STAT_SCORES])
		return; // scoreboard up

	if (ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE])
		return; // spectating

	if (center_print.time > cgi.client->time)
		return;

	if (cg_draw_crosshair->modified) { // crosshair image
		cg_draw_crosshair->modified = false;

		if (cg_draw_crosshair->value < 0)
			cg_draw_crosshair->value = 1;

		if (cg_draw_crosshair->value > 100)
			cg_draw_crosshair->value = 100;

		snprintf(crosshair.name, sizeof(crosshair.name), "ch%d", cg_draw_crosshair->integer);

		crosshair.image = cgi.LoadImage(va("pics/ch%d", cg_draw_crosshair->integer), it_pic);

		if (crosshair.image->type == it_null) {
			cgi.Print("Couldn't load pics/ch%d.\n", cg_draw_crosshair->integer);
			return;
		}
	}

	if (crosshair.image->type == it_null) { // not found
		return;
	}

	if (cg_draw_crosshair_color->modified) { // crosshair color
		cg_draw_crosshair_color->modified = false;

		color = ColorByName(cg_draw_crosshair_color->string, 14);
		memcpy(&crosshair.color, &cgi.palette[color], sizeof(crosshair.color));
	}

	cgi.Colorb(crosshair.color);

	// calculate width and height based on crosshair image and scale
	x = (cgi.context->width - crosshair.image->width * cg_draw_crosshair_scale->value) / 2;
	y = (cgi.context->height - crosshair.image->height * cg_draw_crosshair_scale->value) / 2;

	cgi.DrawPic(x, y, cg_draw_crosshair_scale->value, crosshair.name);

	cgi.Colorb(NULL);
}

/*
 * Cg_ParseCenterPrint
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
	center_print.time = cgi.client->time + 3000;
}

/*
 * Cg_DrawCenterPrint
 */
static void Cg_DrawCenterPrint(const player_state_t *ps) {
	r_pixel_t cw, ch, x, y;
	char *line = center_print.lines[0];

	if (ps->stats[STAT_SCORES])
		return;

	if (center_print.time < cgi.client->time)
		return;

	cgi.BindFont(NULL, &cw, &ch);
	y = (cgi.context->height - center_print.num_lines * ch) / 2;

	while (*line) {
		x = (cgi.context->width - cgi.StringWidth(line)) / 2;

		cgi.DrawString(x, y, line, CON_COLOR_DEFAULT);
		line += MAX_STRING_CHARS;
		y += ch;
	}
}

/**
 * Cg_DrawBlend
 *
 * Draw a full-screen blend effect based on world interaction.
 */
static void Cg_DrawBlend(const player_state_t *ps) {
	static int16_t pickup;
	static uint32_t last_blend_time;
	static int32_t color;
	static float alpha;

	if (!cg_draw_blend->value)
		return;

	if (last_blend_time > cgi.client->time)
		last_blend_time = 0;

	// determine if we've picked up an item
	const int16_t p = ps->stats[STAT_PICKUP_ICON];

	if (p && (p != pickup)) {
		last_blend_time = cgi.client->time;
		color = 215;
		alpha = 0.3;
	}
	pickup = p;

	// or taken damage
	const int16_t d = ps->stats[STAT_DAMAGE_ARMOR] + ps->stats[STAT_DAMAGE_HEALTH];

	if (d) {
		last_blend_time = cgi.client->time;
		color = 240;
		alpha = 0.3;
	}

	// determine the current blend color based on the above events
	float t = (float) (cgi.client->time - last_blend_time) / 500.0;
	float al = cg_draw_blend->value * (alpha - (t * alpha));

	if (al < 0.0 || al > 1.0)
		al = 0.0;

	// and finally, determine supplementary blend based on view origin conents
	const int32_t contents = cgi.view->contents;

	if (al < 0.3 * cg_draw_blend->value && (contents & MASK_WATER)) {
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
		cgi.DrawFill(cgi.view->x, cgi.view->y, cgi.view->width, cgi.view->height, color, al);
	}
}

/*
 * Cg_DrawDamageInflicted
 */
static void Cg_DrawDamageInflicted(const player_state_t *ps) {

	if (ps->stats[STAT_DAMAGE_INFLICT]) {
		static uint32_t last_damage_time;

		// wrap timer around level changes
		if (last_damage_time > cgi.client->time) {
			last_damage_time = 0;
		}

		// play the hit sound
		if (cgi.client->time - last_damage_time > 50) {
			cgi.PlaySample(cgi.view->origin, 0, cg_sample_hit, ATTN_NONE);
			last_damage_time = cgi.client->time;
		}

		// TODO: It would be cool to play different sounds based on the amount
		// of damage inflicted; announcer statements would be bad-ass too.
	}
}

/**
 * Cg_DrawHud
 *
 * Draws the HUD for the current frame.
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
