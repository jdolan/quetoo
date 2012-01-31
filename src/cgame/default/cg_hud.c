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

typedef struct crosshair_s {
	char name[16];
	r_image_t *image;
	byte color[4];
} crosshair_t;

static crosshair_t crosshair;

byte color_white[4] = { 255, 255, 255, 255 };

/*
 * Cg_DrawIcon
 *
 * Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(const r_pixel_t x, const r_pixel_t y,
		const float scale, const unsigned short icon) {

	if (icon >= MAX_IMAGES) {
		cgi.Warn("Cg_DrawIcon: Invalid icon: %d\n", icon);
		return;
	}

	cgi.DrawPic(x, y, scale, cgi.ConfigString(CS_IMAGES + icon));
}

/*
 * Cg_DrawVital
 *
 * Draws the vital numeric and icon, flashing on low quantities.
 */
static void Cg_DrawVital(r_pixel_t x, const short value, const short icon,
		short med, short low) {
	const boolean_t flash = (*cgi.time / 100) & 1;

	r_pixel_t y = *cgi.y + *cgi.h - HUD_PIC_HEIGHT + 4;
	int color = COLOR_HUD_STAT;

	const char *string = va("%3d", value);

	if (value < low) {

		if (!flash) // don't draw at all
			return;

		color = COLOR_HUD_STAT_LOW;
	} else if (value < med) {
		color = COLOR_HUD_STAT_MED;
	}

	x += *cgi.x;
	cgi.DrawString(x, y, string, color);

	x += cgi.StringWidth(string);
	y = *cgi.y + *cgi.h - HUD_PIC_HEIGHT;

	Cg_DrawIcon(x, y, 1.0, icon);
}

/*
 * Cg_DrawVitals
 *
 * Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(const player_state_t *ps) {
	r_pixel_t x, cw, x_offset;

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if (ps->stats[STAT_HEALTH] > 0) {
		const short health = ps->stats[STAT_HEALTH];
		const short health_icon = ps->stats[STAT_HEALTH_ICON];

		x = *cgi.w * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if (ps->stats[STAT_AMMO] > 0) {
		const short ammo = ps->stats[STAT_AMMO];
		const short ammo_low = ps->stats[STAT_AMMO_LOW];
		const short ammo_icon = ps->stats[STAT_AMMO_ICON];

		x = *cgi.w * 0.5 - x_offset;

		Cg_DrawVital(x, ammo, ammo_icon, -1, ammo_low);
	}

	if (ps->stats[STAT_ARMOR] > 0) {
		const short armor = ps->stats[STAT_ARMOR];
		const short armor_icon = ps->stats[STAT_ARMOR_ICON];

		x = *cgi.w * 0.75 - x_offset;

		Cg_DrawVital(x, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
	}

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawPickup
 */
static void Cg_DrawPickup(const player_state_t *ps) {
	r_pixel_t x, y, cw, ch;

	cgi.BindFont(NULL, &cw, &ch);

	if (ps->stats[STAT_PICKUP_ICON] > 0) {
		const short icon = ps->stats[STAT_PICKUP_ICON];
		const short pickup = ps->stats[STAT_PICKUP_STRING];

		const char *string = cgi.ConfigString(pickup);

		x = *cgi.x + *cgi.w - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = *cgi.y;

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
	const short frags = ps->stats[STAT_FRAGS];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.x + *cgi.w - cgi.StringWidth("Frags");
	y = *cgi.y + HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Frags", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = *cgi.x + *cgi.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", frags), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawCaptures
 */
static void Cg_DrawCaptures(const player_state_t *ps) {
	const short captures = ps->stats[STAT_CAPTURES];
	r_pixel_t x, y, cw, ch;

	if (ps->stats[STAT_SPECTATOR])
		return;

	if (atoi(cgi.ConfigString(CS_CTF)) < 1)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.x + *cgi.w - cgi.StringWidth("Captures");
	y = *cgi.y + 2 * HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, "Captures", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = *cgi.x + *cgi.w - 3 * cw;

	cgi.DrawString(x, y, va("%3d", captures), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawSpectator
 */
static void Cg_DrawSpectator(const player_state_t *ps) {
	r_pixel_t x, y, cw;

	if (!ps->stats[STAT_SPECTATOR])
		return;

	cgi.BindFont("small", &cw, NULL);

	x = *cgi.w - cgi.StringWidth("Spectating");
	y = *cgi.y + HUD_PIC_HEIGHT;

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

	const int c = ps->stats[STAT_CHASE];

	if (c - CS_CLIENT_INFO >= MAX_CLIENTS) {
		cgi.Warn("Cg_DrawChase: Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Chasing ^7%s", cgi.ConfigString(c));

	if ((s = strchr(string, '\\')))
		*s = '\0';

	x = *cgi.x + *cgi.w * 0.5 - cgi.StringWidth(string) / 2;
	y = *cgi.y + *cgi.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawVote
 */
static void Cg_DrawVote(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char string[MAX_STRING_CHARS];

	if (!ps->stats[STAT_VOTE])
		return;

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Vote: ^7%s", cgi.ConfigString(ps->stats[STAT_VOTE]));

	x = *cgi.x;
	y = *cgi.y + *cgi.h - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawTime
 */
static void Cg_DrawTime(const player_state_t *ps) {
	r_pixel_t x, y, ch;
	char *string = cgi.ConfigString(CS_TIME);

	if (!ps->stats[STAT_TIME])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.x + *cgi.w - cgi.StringWidth(string);
	y = *cgi.y + HUD_PIC_HEIGHT * 2 + ch;

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

	x = *cgi.x + *cgi.w - cgi.StringWidth("Ready");
	y = *cgi.y + HUD_PIC_HEIGHT * 2 + 2 * ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}

/*
 * Cg_DrawTeamBanner
 */
static void Cg_DrawTeam(const player_state_t *ps) {
	const short team = ps->stats[STAT_TEAM];
	r_pixel_t x, y;
	int color;

	if (!team)
		return;

	if (team == CS_TEAM_GOOD)
		color = 243;
	else if (team == CS_TEAM_EVIL)
		color = 242;
	else {
		cgi.Warn("Cg_DrawTeamBanner: unknown team: %d.\n", team);
		return;
	}

	x = *cgi.x;
	y = *cgi.y + *cgi.h - 64;

	cgi.DrawFill(x, y, *cgi.w, 64, color, 0.15);
}

/*
 * Cg_DrawCrosshair
 */
static void Cg_DrawCrosshair(const player_state_t *ps) {
	r_pixel_t x, y;
	int color;

	if (!cg_crosshair->value)
		return;

	if (ps->stats[STAT_SCORES])
		return; // scoreboard up

	if (ps->stats[STAT_SPECTATOR])
		return; // spectating

	if (ps->stats[STAT_CHASE])
		return; // chasecam

	if (cg_third_person->value)
		return; // third person

	if (cg_crosshair->modified) { // crosshair image
		cg_crosshair->modified = false;

		if (cg_crosshair->value < 0)
			cg_crosshair->value = 1;

		if (cg_crosshair->value > 100)
			cg_crosshair->value = 100;

		snprintf(crosshair.name, sizeof(crosshair.name), "ch%d", cg_crosshair->integer);

		crosshair.image = cgi.LoadPic(crosshair.name);

		if (crosshair.image->type == it_null) {
			cgi.Print("Couldn't load pics/ch%d.\n", cg_crosshair->integer);
			return;
		}
	}

	if (crosshair.image->type == it_null) { // not found
		return;
	}

	if (cg_crosshair_color->modified) { // crosshair color
		cg_crosshair_color->modified = false;

		color = ColorByName(cg_crosshair_color->string, 14);
		memcpy(&crosshair.color, &cgi.palette[color], sizeof(crosshair.color));
	}

	glColor4ubv(crosshair.color);

	// calculate width and height based on crosshair image and scale
	x = (*cgi.width - crosshair.image->width * cg_crosshair_scale->value) / 2;
	y = (*cgi.height - crosshair.image->height * cg_crosshair_scale->value) / 2;

	cgi.DrawPic(x, y, cg_crosshair_scale->value, crosshair.name);

	glColor4ubv(color_white);
}

/*
 * Cg_DrawBlend
 */
static void Cg_DrawBlend(const player_state_t *ps) {
	static short h, a, p;
	static unsigned int last_blend_time;
	static int color;
	static float alpha;
	short dh, da, dp;
	float al, t;

	if (!cg_blend->value)
		return;

	if (last_blend_time > *cgi.time)
		last_blend_time = 0;

	// determine if we've taken damage or picked up an item
	dh = ps->stats[STAT_HEALTH];
	da = ps->stats[STAT_ARMOR];
	dp = ps->stats[STAT_PICKUP_STRING];

	if (ps->pmove.pm_type == PM_NORMAL) {

		if (dp && (dp != p)) { // picked up an item
			last_blend_time = *cgi.time;
			color = 215;
			alpha = 0.3;
		}

		if (da < a) { // took damage
			last_blend_time = *cgi.time;
			color = 240;
			alpha = 0.3;
		}

		if (dh < h) { // took damage
			last_blend_time = *cgi.time;
			color = 240;
			alpha = 0.3;
		}
	}

	al = 0;
	t = (float) (*cgi.time - last_blend_time) / 500.0;
	al = cg_blend->value * (alpha - (t * alpha));

	if (al < 0 || al > 1.0)
		al = 0;

	if (al < 0.3 && (ps->pmove.pm_flags & PMF_UNDER_WATER)) {
		if (al < 0.15 * cg_blend->value)
			color = 114;
		al = 0.3 * cg_blend->value;
	}

	if (al > 0.0)
		cgi.DrawFill(*cgi.x, *cgi.y, *cgi.w, *cgi.h, color, al);

	h = dh; // update our copies
	a = da;
	p = dp;
}

/*
 * Cg_DrawHud
 *
 * Draws the HUD for the current frame.
 */
void Cg_DrawHud(const player_state_t *ps) {

	if (!cg_hud->integer)
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

	Cg_DrawBlend(ps);
}
