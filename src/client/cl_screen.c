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

#include "cl_local.h"

#define NET_GRAPH_HEIGHT 64
#define NET_GRAPH_WIDTH 128
#define NET_GRAPH_Y 0

// net graph samples
typedef struct {
	float value;
	int32_t color;
} net_graph_sample_t;

static net_graph_sample_t net_graph_samples[NET_GRAPH_WIDTH];
static int32_t num_net_graph_samples;

/*
 * @brief Accumulates a net graph sample.
 */
static void Cl_NetGraph(float value, int32_t color) {

	net_graph_samples[num_net_graph_samples].value = value;
	net_graph_samples[num_net_graph_samples].color = color;

	if (net_graph_samples[num_net_graph_samples].value > 1.0)
		net_graph_samples[num_net_graph_samples].value = 1.0;

	num_net_graph_samples++;

	if (num_net_graph_samples == NET_GRAPH_WIDTH)
		num_net_graph_samples = 0;
}

/*
 * @brief Accumulates net graph samples for the current frame. Dropped or
 * suppressed packets are recorded as peak samples, and packet latency is
 * recorded over a range of 0-300ms.
 */
void Cl_AddNetGraph(void) {
	uint32_t i;
	uint32_t in;
	uint32_t ping;

	// we only need to do our accounting when asked to
	if (!cl_draw_net_graph->value)
		return;

	for (i = 0; i < cls.netchan.dropped; i++)
		Cl_NetGraph(1.0, 0x40);

	for (i = 0; i < cl.surpress_count; i++)
		Cl_NetGraph(1.0, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	ping = cls.real_time - cl.cmd_time[in];

	Cl_NetGraph(ping / 300.0, 0xd0); // 300ms is lagged out
}

/*
 * @brief Provides a real-time visual representation of latency and packet loss
 * via a small graph drawn on screen.
 */
static void Cl_DrawNetGraph(void) {
	int32_t i, j, x, y, h;

	if (!cl_draw_net_graph->value)
		return;

	x = r_context.width - NET_GRAPH_WIDTH;
	y = r_context.height - NET_GRAPH_Y - NET_GRAPH_HEIGHT;

	R_DrawFill(x, y, NET_GRAPH_WIDTH, NET_GRAPH_HEIGHT, 8, 0.2);

	for (i = 0; i < NET_GRAPH_WIDTH; i++) {

		j = (num_net_graph_samples - i) & (NET_GRAPH_WIDTH - 1);
		h = net_graph_samples[j].value * NET_GRAPH_HEIGHT;

		if (!h)
			continue;

		x = r_context.width - i;
		y = r_context.height - NET_GRAPH_Y - h;

		R_DrawFill(x, y, 1, h, net_graph_samples[j].color, 0.5);
	}
}

/*
 * @brief
 */
static void Cl_DrawRendererStats(void) {
	r_pixel_t ch, y = 64;

	if (!cl_show_renderer_stats->value)
		return;

	if (cls.state != CL_ACTIVE)
		return;

	R_BindFont("small", NULL, &ch);

	R_DrawString(0, y, "Materials:", CON_COLOR_GREEN);
	y += ch;

	const uint32_t num_bind_diffuse = r_view.num_bind_texture - r_view.num_bind_lightmap
				- r_view.num_bind_deluxemap - r_view.num_bind_normalmap - r_view.num_bind_glossmap;

	R_DrawString(0, y, va("%d diffuse", num_bind_diffuse), CON_COLOR_GREEN);
	y += ch;

	R_DrawString(0, y, va("%d lightmap", r_view.num_bind_lightmap), CON_COLOR_GREEN);
	y += ch;

	R_DrawString(0, y, va("%d deluxemap", r_view.num_bind_deluxemap), CON_COLOR_GREEN);
	y += ch;

	R_DrawString(0, y, va("%d normalmap", r_view.num_bind_normalmap), CON_COLOR_GREEN);
	y += ch;

	R_DrawString(0, y, va("%d glossmap", r_view.num_bind_glossmap), CON_COLOR_GREEN);
	y += ch;

	y += ch;
	R_DrawString(0, y, "BSP:", CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d clusters", r_view.num_bsp_clusters), CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d leafs", r_view.num_bsp_leafs), CON_COLOR_YELLOW);
	y += ch;

	R_DrawString(0, y, va("%d surfaces", r_view.num_bsp_surfaces), CON_COLOR_YELLOW);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Mesh:", CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d models", r_view.num_mesh_models), CON_COLOR_CYAN);
	y += ch;

	R_DrawString(0, y, va("%d tris", r_view.num_mesh_tris), CON_COLOR_CYAN);
	y += ch;

	y += ch;
	R_DrawString(0, y, "Other:", CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d lights", r_view.num_lights), CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d coronas", r_view.num_coronas), CON_COLOR_WHITE);
	y += ch;

	R_DrawString(0, y, va("%d particles", r_view.num_particles), CON_COLOR_WHITE);

	R_BindFont(NULL, NULL, NULL );
}

/*
 * @brief
 */
static void Cl_DrawCounters(void) {
	static vec3_t velocity;
	static char bps[8], pps[8], fps[8], spd[8];
	static int32_t last_draw_time;
	r_pixel_t cw, ch;

	if (!cl_draw_counters->value)
		return;

	R_BindFont("small", &cw, &ch);

	const r_pixel_t x = r_context.width - 7 * cw;
	r_pixel_t y = r_context.height - 4 * ch;

	cl.frame_counter++;

	if (cls.real_time - last_draw_time >= 200) {

		UnpackPosition(cl.frame.ps.pm_state.velocity, velocity);
		velocity[2] = 0.0;

		g_snprintf(spd, sizeof(spd), "%4.0fspd", VectorLength(velocity));
		g_snprintf(fps, sizeof(fps), "%4ufps", cl.frame_counter * 5);
		g_snprintf(pps, sizeof(pps), "%4upps", cl.packet_counter * 5);
		g_snprintf(bps, sizeof(bps), "%4ubps", cl.byte_counter * 5);

		last_draw_time = quake2world.time;

		cl.frame_counter = 0;
		cl.packet_counter = 0;
		cl.byte_counter = 0;
	}

	R_DrawString(x, y, spd, CON_COLOR_DEFAULT);
	y += ch;

	R_DrawString(x, y, fps, CON_COLOR_DEFAULT);
	y += ch;

	R_DrawString(x, y, pps, CON_COLOR_DEFAULT);
	y += ch;

	R_DrawString(x, y, bps, CON_COLOR_DEFAULT);

	R_BindFont(NULL, NULL, NULL );
}

/*
 * @brief
 */
static void Cl_DrawCursor(void) {

	if (cls.key_state.dest != KEY_UI && cls.mouse_state.grabbed)
		return;

	if (!(SDL_GetAppState() & SDL_APPMOUSEFOCUS))
		return;

	R_DrawCursor(cls.mouse_state.x, cls.mouse_state.y);
}

/*
 * @brief This is called every frame, and can also be called explicitly to flush
 * text to the screen.
 */
void Cl_UpdateScreen(void) {

	R_BeginFrame();

	if (cls.state == CL_ACTIVE && !cls.loading) {

		Cl_UpdateView();

		R_Setup3D();

		R_DrawView();

		R_Setup2D();

		if (cls.key_state.dest != KEY_CONSOLE && cls.key_state.dest != KEY_UI) {

			Cl_DrawNetGraph();

			Cl_DrawCounters();

			Cl_DrawNotify();

			Cl_DrawRendererStats();

			cls.cgame->DrawFrame(&cl.frame);
		}
	} else {
		R_Setup2D();
	}

	Cl_DrawConsole();

	R_Draw2D(); // draw all 2D geometry for the frame

	Ui_Draw();

	Cl_DrawCursor();

	R_EndFrame();
}
