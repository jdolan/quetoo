/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "client.h"

#define COLOR_HUD_STAT         CON_COLOR_DEFAULT
#define COLOR_HUD_STAT_MED     CON_COLOR_YELLOW
#define COLOR_HUD_STAT_LOW     CON_COLOR_RED
#define COLOR_HUD_STAT_STRING  CON_COLOR_DEFAULT
#define COLOR_HUD_COUNTER      CON_COLOR_DEFAULT
#define COLOR_SCORES_HEADER    CON_COLOR_ALT

#define NETGRAPH_HEIGHT 64
#define NETGRAPH_WIDTH 128
#define NETGRAPH_Y 128

// netgraph samples
typedef struct {
	float value;
	int color;
} graphsamp_t;

static graphsamp_t graphsamps[NETGRAPH_WIDTH];
static int num_graphsamps;


/*
 * Cl_Netgraph
 */
static void Cl_Netgraph(float value, int color){

	graphsamps[num_graphsamps].value = value;
	graphsamps[num_graphsamps].color = color;

	if(graphsamps[num_graphsamps].value > 1.0)
		graphsamps[num_graphsamps].value = 1.0;

	num_graphsamps++;

	if(num_graphsamps == NETGRAPH_WIDTH)
		num_graphsamps = 0;
}


/*
 * Cl_AddNetgraph
 */
void Cl_AddNetgraph(void){
	int i;
	int in;
	int ping;

	// we only need to do our accounting when asked to
	if(!cl_netgraph->value)
		return;

	for(i = 0; i < cls.netchan.dropped; i++)
		Cl_Netgraph(1.0, 0x40);

	for(i = 0; i < cl.surpress_count; i++)
		Cl_Netgraph(1.0, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	ping = cls.realtime - cl.cmd_time[in];

	Cl_Netgraph(ping / 1000.0, 0xd0);  // 1000ms is lagged out
}


/*
 * Cl_DrawNetgraph
 */
static void Cl_DrawNetgraph(void){
	int i, j, h;

	if(!cl_netgraph->value)
		return;

	R_DrawFillAlpha(r_state.width - NETGRAPH_WIDTH, r_state.height - NETGRAPH_Y - NETGRAPH_HEIGHT,
			NETGRAPH_WIDTH, NETGRAPH_HEIGHT, 8, 0.2);

	for(i = 0; i < NETGRAPH_WIDTH; i++){

		j = (num_graphsamps - i) & (NETGRAPH_WIDTH - 1);
		h = graphsamps[j].value * NETGRAPH_HEIGHT;

		if(!h)
			continue;

		R_DrawFill(r_state.width - i, r_state.height - NETGRAPH_Y - h, 1, h, graphsamps[j].color);
	}
}


/*
 * Cl_DrawTeamBanner
 */
static void Cl_DrawTeamBanner(void){
	int color;
	const int team = cl.frame.playerstate.stats[STAT_TEAMNAME];

	if(!team)
		return;

	if(team == CS_TEAMGOOD)
		color = 243;
	else if(team == CS_TEAMEVIL)
		color = 242;
	else {
		Com_Warn("Cl_DrawTeamBanner: unknown team: %d.\n", team);
		return;
	}

	R_DrawFillAlpha(0, r_state.height - 96, r_state.width, r_state.height, color, 0.15);
}


char centerstring[MAX_STRING_CHARS];
float centertime;
int centerlines;

/*
 * Cl_CenterPrint
 */
void Cl_CenterPrint(char *str){
	char *s;

	strncpy(centerstring, str, sizeof(centerstring) - 1);

	centertime = cl.time + 5.0;

	// count the number of lines for centering
	centerlines = 1;
	s = str;
	while(*s){
		if(*s == '\n')
			centerlines++;
		s++;
	}

	Com_Printf("%s", str);

	Con_ClearNotify();
}


/*
 * Cl_DrawCenterString
 */
static void Cl_DrawCenterString(void){
	const char *s;
	int x, y, size, len;

	s = centerstring;

	if(centerlines <= 4)
		y = r_state.height * 0.35;
	else
		y = 48;

	do {
		// scan the width of the line, ignoring color keys
		len = size = 0;
		while(true){

			if(!s[size] || s[size] == '\n' || len >= 40)
				break;

			if(IS_COLOR(&s[size])){
				size += 2;
				continue;
			}

			size++;
			len++;
		}

		x = (r_state.width - (len * 16)) / 2;

		// draw it
		R_DrawSizedString(x, y, s, len, 999, CON_COLOR_DEFAULT);

		// look for next line
		s += size;
		while(*s && *s != '\n')
			s++;

		if(!*s)
			return;

		s++;  // skip the \n

		y += 32;
	} while(true);
}


/*
 * Cl_CheckDrawCenterString
 */
static void Cl_CheckDrawCenterString(void){

	if(centertime <= cl.time)
		return;

	Cl_DrawCenterString();
}


/*
 * Cl_DrawConsoleOrNotify
 */
static void Cl_DrawConsoleOrNotify(void){

	if(cls.state == ca_disconnected || cls.state == ca_connecting){
		if(cls.download.http)
			Con_DrawConsole(0.5);
		else
			Con_DrawConsole(1.0);
		return;
	}

	if(cls.key_dest == key_console || cls.state == ca_connected){
		Con_DrawConsole(0.5);
		return;
	}

	if(cls.key_dest == key_game || cls.key_dest == key_message)
		Con_DrawNotify();  // only draw notify in game

	// a little hacky, but because of where we want rspeeds to show up
	// this is the best place for it
	if(r_speeds->value){
		char rspeeds[128];

		sprintf(rspeeds, "%i bsp %i mesh %i lights %i parts",
				r_view.bsp_polys, r_view.mesh_polys, r_view.num_lights, r_view.num_particles);

		R_DrawString(r_state.width - strlen(rspeeds) * 16, 0, rspeeds, CON_COLOR_YELLOW);
	}
}


/*
 * Cl_DrawHUDString
 */
static void Cl_DrawHUDString(const char *string, int x, int y, int centerwidth, int color){
	int margin;
	char line[MAX_STRING_CHARS];
	int width;
	int i;

	margin = x;

	while(*string){
		// scan out one line of text from the string
		width = 0;
		while(*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		if(centerwidth)
			x = margin + (centerwidth - width * 16) / 2;
		else
			x = margin;
		for(i = 0; i < width; i++){
			R_DrawChar(x, y, line[i] ,  color);
			x += 16;
		}
		if(*string){
			string++;  // skip the \n
			x = margin;
			y += 32;
		}
	}
}


/*
 * Cl_ExecuteLayoutString
 */
static void Cl_ExecuteLayoutString(const char *s){
	int x, y;
	int value, value2;
	const char *token;
	int index;
	char *c;
	char string[MAX_STRING_CHARS];
	qboolean flash;

	if(cls.state != ca_active)
		return;

	if(!cl_hud->value || !s[0])
		return;

	x = 0;
	y = 0;

	flash = (cl.time / 100) & 1;  // for flashing fields

	while(s){
		token = Com_Parse(&s);
		if(!strcmp(token, "xl")){
			token = Com_Parse(&s);
			x = atoi(token);
			continue;
		}
		if(!strcmp(token, "xr")){
			token = Com_Parse(&s);
			x = r_state.width + atoi(token);
			continue;
		}
		if(!strcmp(token, "xv")){
			token = Com_Parse(&s);
			x = r_state.width / 2 - 320 + atoi(token);
			continue;
		}

		if(!strcmp(token, "yt")){
			token = Com_Parse(&s);
			y = atoi(token);
			continue;
		}
		if(!strcmp(token, "yb")){
			token = Com_Parse(&s);
			y = r_state.height + atoi(token);
			continue;
		}
		if(!strcmp(token, "yv")){
			token = Com_Parse(&s);
			y = r_state.height / 2 - 240 + atoi(token);
			continue;
		}

		if(!strcmp(token, "pic")){  // draw a pic from a stat number
			token = Com_Parse(&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if(value >= MAX_IMAGES)
				Com_Warn("Cl_ExecuteLayoutString: pic %d >= MAX_IMAGES\n", value);
			else if(cl.configstrings[CS_IMAGES + value]){
				R_DrawPic(x, y, cl.configstrings[CS_IMAGES + value]);
			}
			continue;
		}

		if(!strcmp(token, "health")){  // health number
			value = cl.frame.playerstate.stats[STAT_HEALTH];
			if(value > 0){
				if (value >= 80)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT);
				else if (value >= 40)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT_MED);
				else if (flash)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT_LOW);
			}
			continue;
		}

		if(!strcmp(token, "ammo")){  // ammo number
			value = cl.frame.playerstate.stats[STAT_AMMO];
			value2 = cl.frame.playerstate.stats[STAT_AMMO_LOW];
			if(value > 0){
				if (value >= value2)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT);
				else if (flash)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT_LOW);
			}
			continue;
		}

		if(!strcmp(token, "armor")){  // armor number
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if(value > 0){
				if (value >= 80)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT);
				else if (value >= 40)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT_MED);
				else if (flash)
					R_DrawString(x, y, va("%3i", value), COLOR_HUD_STAT_LOW);
			}
			continue;
		}

		if(!strcmp(token, "stat_string")){
			token = Com_Parse(&s);
			index = atoi(token);
			if(index < 0 || index >= MAX_CONFIGSTRINGS){
				Com_Warn("Cl_ExecuteLayoutString: Invalid stat_string index: %i.\n", index);
				continue;
			}
			index = cl.frame.playerstate.stats[index];
			if(index < 0 || index >= MAX_CONFIGSTRINGS){
				Com_Warn("Cl_ExecuteLayoutString: Bad stat_string index: %i.\n", index);
				continue;
			}
			strncpy(string, cl.configstrings[index], sizeof(string) - 1);
			if((c = strchr(string, '\\')))  // mute chasecam skins
				*c = 0;
			R_DrawString(x, y, string, CON_COLOR_DEFAULT);
			continue;
		}

		if(!strcmp(token, "cstring")){
			token = Com_Parse(&s);
			Cl_DrawHUDString(token, x, y, 320, CON_COLOR_DEFAULT);
			continue;
		}

		if(!strcmp(token, "string")){
			token = Com_Parse(&s);
			R_DrawString(x, y, token, CON_COLOR_DEFAULT);
			continue;
		}

		if(!strcmp(token, "cstring2")){
			token = Com_Parse(&s);
			Cl_DrawHUDString(token, x, y, 320, CON_COLOR_ALT);
			continue;
		}

		if(!strcmp(token, "string2")){
			token = Com_Parse(&s);
			R_DrawString(x, y, token, COLOR_SCORES_HEADER);
			continue;
		}

		if(!strcmp(token, "if")){  // conditional
			token = Com_Parse(&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if(!value){  // skip to endif
				while(s && strcmp(token, "endif")){
					token = Com_Parse(&s);
				}
			}

			continue;
		}
	}
}


typedef struct crosshair_s {
	char name[16];
	int width, height;
	byte color[4];
} crosshair_t;

static crosshair_t crosshair;

/*
 * Cl_DrawCrosshair
 */
static void Cl_DrawCrosshair(void){
	image_t *image;
	int offset, w, h;

	if(!cl_crosshair->value)
		return;

	if(cls.state != ca_active)
		return;  // not spawned yet

	if(cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;  // scoreboard up

	if(cl.frame.playerstate.stats[STAT_SPECTATOR])
		return;  // spectating

	if(cl.frame.playerstate.stats[STAT_CHASE])
		return;  // chasecam

	if(cl_crosshair->modified){  // crosshair image
		cl_crosshair->modified = false;

		crosshair.width = 0;

		if(cl_crosshair->value < 0)
			cl_crosshair->value = 1;

		if(cl_crosshair->value > 100)
			cl_crosshair->value = 100;

		snprintf(crosshair.name, sizeof(crosshair.name), "ch%d", (int)cl_crosshair->value);

		image = R_LoadPic(crosshair.name);

		if(image == r_notexture){
			Com_Printf("Couldn't load pics/ch%d.\n", (int)cl_crosshair->value);
			return;
		}

		crosshair.width = image->width;
		crosshair.height = image->height;
	}

	if(!crosshair.width)  // not found
		return;

	if(cl_crosshaircolor->modified){  // crosshair color
		cl_crosshaircolor->modified = false;

		*(int *)crosshair.color = palette[ColorByName(cl_crosshaircolor->string, 14)];
	}

	glColor4ubv(crosshair.color);

	// adjust the crosshair for 3rd person perspective
	offset = cl_thirdperson->value * 0.008 * r_view.height;

	// calculate width and height based on crosshair image and scale
	w = (r_view.width - crosshair.width * cl_crosshairscale->value) / 2;
	h = (r_view.height - crosshair.height * cl_crosshairscale->value) / 2;

	R_DrawScaledPic(r_view.x + w, r_view.y + h + offset,
			cl_crosshairscale->value, crosshair.name);

	glColor4ubv(color_white);
}


/*
 * Cl_DrawStats
 *
 * The status bar is a small layout program based on the stats array
 */
static void Cl_DrawStats(void){
	Cl_ExecuteLayoutString(cl.configstrings[CS_STATUSBAR]);
}


/*
 * Cl_DrawLayout
 */
static void Cl_DrawLayout(void){

	if(!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;

	if(cls.key_dest == key_console)
		return;

	Cl_ExecuteLayoutString(cl.layout);
}


int frames_this_second = 0, packets_this_second = 0, bytes_this_second = 0;

/*
 * Cl_DrawCounters
 */
static void Cl_DrawCounters(void){
	static vec3_t velocity;
	static char bps[8], pps[8], fps[8], spd[8];
	static int millis;

	if(!cl_counters->value)
		return;

	if(timedemo->value){  // account for timedemo frames
		if(!cl.timedemo_start)
			cl.timedemo_start = cls.realtime;
		cl.timedemo_frames++;
	}

	frames_this_second++;

	if(curtime - millis >= 1000){

		VectorCopy(r_view.velocity, velocity);
		velocity[2] = 0.0;

		snprintf(spd, sizeof(spd), "%4.0fspd", VectorLength(velocity));
		snprintf(fps, sizeof(fps), "%4dfps", frames_this_second);
		snprintf(pps, sizeof(pps), "%4dpps", packets_this_second);
		snprintf(bps, sizeof(bps), "%4dbps", bytes_this_second);

		millis = curtime;

		frames_this_second = 0;
		packets_this_second = 0;
		bytes_this_second = 0;
	}

	R_DrawString(r_state.width - 112, r_state.height - 128, spd, COLOR_HUD_COUNTER);
	R_DrawString(r_state.width - 112, r_state.height - 96, fps, COLOR_HUD_COUNTER);
	R_DrawString(r_state.width - 112, r_state.height - 64, pps, COLOR_HUD_COUNTER);
	R_DrawString(r_state.width - 112, r_state.height - 32, bps, COLOR_HUD_COUNTER);
}


/*
 * Cl_DrawBlend
 */
static void Cl_DrawBlend(void){
	static int h, a, p;
	static int last_blend_time;
	static int color;
	static float alpha;
	int dh, da, dp;
	float al, t;

	if(!cl_blend->value)
		return;

	if(last_blend_time > cl.time)
		last_blend_time = 0;

	// determine if we've taken damage or picked up an item
	dh = cl.frame.playerstate.stats[STAT_HEALTH];
	da = cl.frame.playerstate.stats[STAT_ARMOR];
	dp = cl.frame.playerstate.stats[STAT_PICKUP_STRING];

	if(cl.frame.playerstate.pmove.pm_type == PM_NORMAL){

		if(dp && (dp != p)){  // picked up an item
			last_blend_time = cl.time;
			color = 215;
			alpha = 0.3;
		}

		if(da < a){  // took damage
			last_blend_time = cl.time;
			color = 240;
			alpha = 0.3;
		}

		if(dh < h){  // took damage
			last_blend_time = cl.time;
			color = 240;
			alpha = 0.3;
		}
	}

	al = 0;
	t = (float)(cl.time - last_blend_time) / 500.0;
	al = cl_blend->value * (alpha - (t * alpha));

	if(al < 0 || al > 1.0)
		al = 0;

	if(al < 0.3 && cl.underwater){  // underwater view
		if(al < 0.15 * cl_blend->value)
			color = 114;
		al = 0.3 * cl_blend->value;
	}

	if(al > 0.0)
		R_DrawFillAlpha(r_view.x, r_view.y, r_view.width, r_view.height, color, al);

	h = dh;  // update our copies
	a = da;
	p = dp;
}


/*
 * Cl_UpdateScreen
 *
 * This is called every frame, and can also be called explicitly to flush
 * text to the screen.
 */
void Cl_UpdateScreen(void){

	R_BeginFrame();

	if(cls.state == ca_active && r_view.ready){

		Cl_UpdateView();

		R_Setup3D();

		R_DrawFrame();

		R_Setup2D();

		Cl_DrawTeamBanner();

		Cl_DrawNetgraph();

		Cl_DrawCrosshair();

		Cl_DrawStats();

		if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
			Cl_DrawLayout();

		Cl_CheckDrawCenterString();

		Cl_DrawCounters();

		Cl_DrawBlend();
	}
	else {
		R_Setup2D();
	}

	Cl_DrawConsoleOrNotify();

	R_DrawFillAlphas();  // draw all fills accumulated above

	R_DrawChars();  // draw all chars accumulated above

	R_EndFrame();
}
