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


/*
 * Cg_DrawIcon
 *
 * Draws the icon at the specified ConfigString index, relative to CS_IMAGES.
 */
static void Cg_DrawIcon(int x, int y, float scale, short icon){

	if(icon >= MAX_IMAGES){
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
static void Cg_DrawVital(int x, short value, short icon, short med, short low){
	const boolean_t flash = (*cgi.time / 100) & 1;

	int y = *cgi.height - HUD_PIC_HEIGHT + 4;
	int color = COLOR_HUD_STAT;

	const char *string = va("%3d", value);

	if(value < low){

		if(!flash)  // don't draw at all
			return;

		color = COLOR_HUD_STAT_LOW;
	}
	else if(value < med){
		color = COLOR_HUD_STAT_MED;
	}

	cgi.DrawString(x, y, string, color);

	x += cgi.StringWidth(string);
	y = *cgi.height - HUD_PIC_HEIGHT;

	Cg_DrawIcon(x, y, 1.0, icon);
}


/*
 * Cg_DrawVitals
 *
 * Draws health, ammo and armor numerics and icons.
 */
static void Cg_DrawVitals(player_state_t *ps){
	int x, cw, x_offset;

	cgi.BindFont("large", &cw, NULL);

	x_offset = 3 * cw;

	if(ps->stats[STAT_HEALTH] > 0){
		const short health = ps->stats[STAT_HEALTH];
		const short health_icon = ps->stats[STAT_HEALTH_ICON];

		x = *cgi.width * 0.25 - x_offset;

		Cg_DrawVital(x, health, health_icon, HUD_HEALTH_MED, HUD_HEALTH_LOW);
	}

	if(ps->stats[STAT_AMMO] > 0){
		const short ammo = ps->stats[STAT_AMMO];
		const short ammo_low = ps->stats[STAT_AMMO_LOW];
		const short ammo_icon = ps->stats[STAT_AMMO_ICON];

		x = *cgi.width * 0.5 - x_offset;

		Cg_DrawVital(x, ammo, ammo_icon, -1, ammo_low);
	}

	if(ps->stats[STAT_ARMOR] > 0){
		const short armor = ps->stats[STAT_ARMOR];
		const short armor_icon = ps->stats[STAT_ARMOR_ICON];

		x = *cgi.width * 0.75 - x_offset;

		Cg_DrawVital(x, armor, armor_icon, HUD_ARMOR_MED, HUD_ARMOR_LOW);
	}

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawPickup
 */
static void Cg_DrawPickup(player_state_t *ps){
	int x, y, cw, ch;

	cgi.BindFont(NULL, &cw, &ch);

	if(ps->stats[STAT_PICKUP_ICON] > 0){
		const short icon = ps->stats[STAT_PICKUP_ICON];
		const short pickup = ps->stats[STAT_PICKUP_STRING];

		const char *string = cgi.ConfigString(pickup);

		x = *cgi.width - HUD_PIC_HEIGHT - cgi.StringWidth(string);
		y = 0;

		Cg_DrawIcon(x, y, 1.0, icon);

		x += HUD_PIC_HEIGHT;
		y += ch / 2;

		cgi.DrawString(x, y, string, COLOR_HUD_STAT);
	}
}


/*
 * Cg_DrawFrags
 */
static void Cg_DrawFrags(player_state_t *ps){
	const short frags = ps->stats[STAT_FRAGS];
	int x, y, cw, ch;

	if(ps->stats[STAT_SPECTATOR])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.width - cgi.StringWidth("Frags");
	y = HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Frags", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = *cgi.width - 3 * cw;

	cgi.DrawString(x, y, va("%3d", frags), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawCaptures
 */
static void Cg_DrawCaptures(player_state_t *ps){
	const short captures = ps->stats[STAT_CAPTURES];
	int x, y, cw, ch;

	if(ps->stats[STAT_SPECTATOR])
		return;

	if(atoi(cgi.ConfigString(CS_CTF)) < 1)
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.width - cgi.StringWidth("Captures");
	y = 2 * HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, "Captures", CON_COLOR_GREEN);
	y += ch;

	cgi.BindFont("large", &cw, NULL);

	x = *cgi.width - 3 * cw;

	cgi.DrawString(x, y, va("%3d", captures), COLOR_HUD_STAT);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawSpectator
 */
static void Cg_DrawSpectator(player_state_t *ps){
	int x, y, cw;

	if(!ps->stats[STAT_SPECTATOR])
		return;

	cgi.BindFont("small", &cw, NULL);

	x = *cgi.width - cgi.StringWidth("Spectating");
	y = HUD_PIC_HEIGHT;

	cgi.DrawString(x, y, "Spectating", CON_COLOR_GREEN);
}


/*
 * Cg_DrawChase
 */
static void Cg_DrawChase(player_state_t *ps){
	int x, y, ch;
	char string[MAX_USER_INFO_VALUE * 2], *s;

	if(!ps->stats[STAT_CHASE])
		return;

	const int c = ps->stats[STAT_CHASE];

	if(c - CS_CLIENT_INFO >= MAX_CLIENTS){
		cgi.Warn("Cg_DrawChase: Invalid client info index: %d\n", c);
		return;
	}

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Chasing ^7%s", cgi.ConfigString(c));

	if((s = strchr(string, '\\')))
		*s = '\0';

	x = *cgi.width * 0.5 - cgi.StringWidth(string) / 2;
	y = *cgi.height - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawVote
 */
static void Cg_DrawVote(player_state_t *ps){
	int x, y, ch;
	char string[MAX_STRING_CHARS];

	if(!ps->stats[STAT_VOTE])
		return;

	cgi.BindFont("small", NULL, &ch);

	snprintf(string, sizeof(string) - 1, "Vote: ^7%s", cgi.ConfigString(ps->stats[STAT_VOTE]));

	x = 0;
	y = *cgi.height - HUD_PIC_HEIGHT - ch;

	cgi.DrawString(x, y, string, CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawTime
 */
static void Cg_DrawTime(player_state_t *ps){
	int x, y, ch;
	char *string = cgi.ConfigString(CS_TIME);

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.width - cgi.StringWidth(string);
	y = HUD_PIC_HEIGHT * 2 + ch;

	if(atoi(cgi.ConfigString(CS_CTF)) > 0)
		y += HUD_PIC_HEIGHT + ch;

	cgi.DrawString(x, y, string, CON_COLOR_DEFAULT);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawReady
 */
static void Cg_DrawReady(player_state_t *ps){
	int x, y, ch;

	if(!ps->stats[STAT_READY])
		return;

	cgi.BindFont("small", NULL, &ch);

	x = *cgi.width - cgi.StringWidth("Ready");
	y = HUD_PIC_HEIGHT * 2 + 2 * ch;

	cgi.DrawString(x, y, "Ready", CON_COLOR_GREEN);

	cgi.BindFont(NULL, NULL, NULL);
}


/*
 * Cg_DrawTeamBanner
 */
static void Cg_DrawTeam(player_state_t *ps){
	const int team = ps->stats[STAT_TEAM];
	int color;

	if(!team)
		return;

	if(team == CS_TEAM_GOOD)
		color = 243;
	else if(team == CS_TEAM_EVIL)
		color = 242;
	else {
		cgi.Warn("Cg_DrawTeamBanner: unknown team: %d.\n", team);
		return;
	}

	cgi.DrawFill(0, *cgi.height - 64, *cgi.width, *cgi.height, color, 0.15);
}


/*
 * Cg_DrawBlend
 */
static void Cg_DrawBlend(player_state_t *ps){
	static int h, a, p;
	static int last_blend_time;
	static int color;
	static float alpha;
	int dh, da, dp;
	float al, t;

	if(!cg_blend->value)
		return;

	if(last_blend_time > *cgi.time)
		last_blend_time = 0;

	// determine if we've taken damage or picked up an item
	dh = ps->stats[STAT_HEALTH];
	da = ps->stats[STAT_ARMOR];
	dp = ps->stats[STAT_PICKUP_STRING];

	if(ps->pmove.pm_type == PM_NORMAL){

		if(dp && (dp != p)){  // picked up an item
			last_blend_time = *cgi.time;
			color = 215;
			alpha = 0.3;
		}

		if(da < a){  // took damage
			last_blend_time = *cgi.time;
			color = 240;
			alpha = 0.3;
		}

		if(dh < h){  // took damage
			last_blend_time = *cgi.time;
			color = 240;
			alpha = 0.3;
		}
	}

	al = 0;
	t = (float)(*cgi.time - last_blend_time) / 500.0;
	al = cg_blend->value * (alpha - (t * alpha));

	if(al < 0 || al > 1.0)
		al = 0;

	if(al < 0.3 && ps->pmove.pm_flags & PMF_UNDER_WATER){
		if(al < 0.15 * cg_blend->value)
			color = 114;
		al = 0.3 * cg_blend->value;
	}

	if(al > 0.0)
		cgi.DrawFill(0, 0, *cgi.width, *cgi.height, color, al);

	h = dh;  // update our copies
	a = da;
	p = dp;
}


/*
 * Cg_DrawHud
 *
 * Draws the HUD for the current frame.
 */
void Cg_DrawHud(player_state_t *ps){

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


	Cg_DrawBlend(ps);
}
