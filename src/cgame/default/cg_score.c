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

#define SCORES_COL_WIDTH 240
#define SCORES_ROW_HEIGHT 48
#define SCORES_ICON_WIDTH 48

typedef struct {
	g_score_t scores[MAX_CLIENTS + MAX_TEAMS];
	uint16_t num_scores;
} cg_score_state_t;

static cg_score_state_t cg_score_state;

/**
 * @brief A comparator for sorting player_score_t.
 */
static int32_t Cg_ParseScores_Compare(const void *a, const void *b) {
	const g_score_t *sa = (g_score_t *) a;
	const g_score_t *sb = (g_score_t *) b;

	// push spectators to the bottom of the board
	const int16_t s1 = (sa->flags & SCORE_SPECTATOR ? -9999 : sa->score);
	const int16_t s2 = (sb->flags & SCORE_SPECTATOR ? -9999 : sb->score);

	return s2 - s1;
}

/**
 * @brief Parses score data from the server. The scores are sent as binary.
 * If teams play or CTF is enabled, the last two scores in the packet will
 * contain the team scores.
 */
void Cg_ParseScores(void) {

	const size_t index = cgi.ReadShort();
	const size_t count = cgi.ReadShort();

	if (index == 0) {
		memset(&cg_score_state, 0, sizeof(cg_score_state));
	}

	cgi.ReadData(cg_score_state.scores + index, count * sizeof(g_score_t));

	if (cgi.ReadByte()) { // last packet in sequence

		cg_score_state.num_scores = index + count;

		// the aggregate scores are the last set in the array
		if (cg_state.ctf || cg_state.teams) {
			cg_score_state.num_scores -= MAX_TEAMS;
		}
	}

	qsort(cg_score_state.scores, cg_score_state.num_scores, sizeof(g_score_t),
	      Cg_ParseScores_Compare);
}

/**
 * @brief Returns the vertical screen coordinate where scores should be drawn.
 */
static r_pixel_t Cg_DrawScoresHeader(void) {
	r_pixel_t cw, ch, x, y;

	cgi.BindFont("medium", &cw, &ch);

	y = cgi.view->viewport.y + 64 - ch - 4;

	const char *map_name = cgi.ConfigString(CS_NAME);
	const r_pixel_t sw = cgi.StringWidth(map_name);

	// map title
	x = cgi.context->width / 2 - sw / 2;
	cgi.DrawString(x, y, map_name, CON_COLOR_DEFAULT);

	y += ch;

	// team names and scores
	if (cg_state.teams || cg_state.ctf) {
		cgi.BindFont("small", &cw, &ch);

		g_score_t *score = &cg_score_state.scores[cg_score_state.num_scores];

		// start from center
		x = cgi.context->width / 2;
		x -= SCORES_COL_WIDTH * (cg_state.teams / 2.0);
		x += SCORES_ICON_WIDTH;

		for (int32_t i = 0; i < cg_state.teams; i++, score++) {
			int16_t score_num = cg_state.ctf ? score->captures : score->score;
			int32_t color_id;

			// FIXME: pls
			switch (i) {
			case TEAM_RED:
			default:
				color_id = CON_COLOR_RED;
				break;
			case TEAM_BLUE:
				color_id = CON_COLOR_BLUE;
				break;
			case TEAM_GREEN:
				color_id = CON_COLOR_GREEN;
				break;
			case TEAM_ORANGE:
				color_id = CON_COLOR_YELLOW;
				break;
			}

			cgi.DrawString(x, y, va("%s^7 %d %s", cg_team_info[i].team_name, score_num,
									cg_state.ctf ? "caps" : "frags"), color_id);

			x += SCORES_COL_WIDTH;
		}

		y += ch;
	}

	return y;
}

/**
 * @brief
 */
static _Bool Cg_DrawScore(r_pixel_t x, r_pixel_t y, const g_score_t *s) {
	r_pixel_t cw, ch;

	const cl_client_info_t *info = &cgi.client->client_info[s->client];

	// icon
	const vec_t is = (vec_t) (SCORES_ICON_WIDTH - 2) / (vec_t) info->icon->width;
	cgi.DrawImage(x, y, is, info->icon);

	// flag carrier icon
	if (cg_state.ctf && (s->flags & SCORE_CTF_FLAG)) {
		const int32_t team = s->team;
		const r_image_t *flag = cgi.LoadImage(va("pics/i_flag%d", team), IT_PIC);
		cgi.DrawImage(x, y, 0.33, flag);
	}

	x += SCORES_ICON_WIDTH;

	// background
	const vec_t fa = s->client == cgi.client->client_num ? 0.3 : 0.15;
	const r_pixel_t fw = SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 1;
	const r_pixel_t fh = SCORES_ROW_HEIGHT - 1;

	if (s->color != 0) {
		const SDL_Color color = MVC_HSVToRGB(s->color, 1.0, 1.0);
		const color_t c = {
			.r = color.r,
			.g = color.g,
			.b = color.b,
			.a = (fa * 255)
		};

		cgi.DrawFill(x, y, fw, fh, c.u32, -1.0);
	}

	cgi.BindFont("small", &cw, &ch);

	// name
	cgi.DrawString(x, y, info->name, CON_COLOR_DEFAULT);

	// ping
	{
		const r_pixel_t px = x + SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 6 * cw;
		cgi.DrawString(px, y, va("%3dms", s->ping), CON_COLOR_DEFAULT);
		y += ch;
	}

	// spectating
	if (s->flags & SCORE_SPECTATOR) {
		cgi.DrawString(x, y, "spectating", CON_COLOR_DEFAULT);
		return true;
	}

	// frags
	cgi.DrawString(x, y, va("%d frags", s->score), CON_COLOR_DEFAULT);

	// deaths
	char *deaths = va("%d deaths ", s->deaths);
	cgi.DrawString(x + fw - cgi.StringWidth(deaths), y, deaths, CON_COLOR_DEFAULT);
	y += ch;

	// ready/not ready
	if (cg_state.match) {
		if (s->flags & SCORE_NOT_READY) {
			cgi.DrawString(x + fw - cgi.StringWidth("not ready "), y, "not ready", CON_COLOR_DEFAULT);
		}
	}

	// captures
	if (!cg_state.ctf) {
		return true;
	}

	cgi.DrawString(x, y, va("%d captures", s->captures), CON_COLOR_DEFAULT);
	return true;
}

/**
 * @brief
 */
static void Cg_DrawTeamScores(const r_pixel_t start_y) {
	r_pixel_t x, y;
	int16_t rows;
	size_t i;
	int32_t j = 0;

	rows = (cgi.context->height - (2 * start_y)) / SCORES_ROW_HEIGHT;
	rows = rows < 3 ? 3 : rows;

	x = cgi.context->width / 2;
	x -= SCORES_COL_WIDTH * (cg_state.teams / 2.0);

	y = start_y;

	for (int32_t t = 0; t < cg_state.teams; t++, x += SCORES_COL_WIDTH, y = start_y) {
		for (i = 0; i < cg_score_state.num_scores; i++) {
			const g_score_t *s = &cg_score_state.scores[i];

			if (s->team != t + 1) {
				continue;
			}

			if ((int16_t) i == rows) {
				break;
			}

			if (Cg_DrawScore(x, y, s)) {
				y += SCORES_ROW_HEIGHT;
			}
		}
	}

	x = cgi.context->width / 2;
	x -= SCORES_COL_WIDTH * (cg_state.teams / 2.0);
	x -= SCORES_COL_WIDTH * 2.0;
	y = start_y;

	for (i = 0; i < cg_score_state.num_scores; i++) {
		const g_score_t *s = &cg_score_state.scores[i];

		if (!(s->flags & SCORE_SPECTATOR)) {
			continue;
		}

		if ((int16_t) i == rows) {
			break;
		}

		if (Cg_DrawScore(x, y, s)) {
			if (j++ % 2) {
				x -= SCORES_COL_WIDTH;
				y += SCORES_ROW_HEIGHT;
			} else {
				x += SCORES_COL_WIDTH;
			}
		}
	}
}

/**
 * @brief
 */
static void Cg_DrawDmScores(const r_pixel_t start_y) {
	int16_t rows, cols;
	r_pixel_t width;
	size_t i;

	rows = (cgi.context->height - (2 * start_y)) / SCORES_ROW_HEIGHT;
	rows = rows < 3 ? 3 : rows;

	cols = (rows < (int16_t) cg_score_state.num_scores) ? 2 : 1;
	width = cols * SCORES_COL_WIDTH;

	const g_score_t *s = cg_score_state.scores;
	for (i = 0; i < cg_score_state.num_scores; i++, s++) {

		if ((int16_t) i == (cols * rows)) { // screen is full
			break;
		}

		const int16_t col = i / rows;

		const r_pixel_t x = cgi.context->width / 2 - width / 2 + col * SCORES_COL_WIDTH;
		const r_pixel_t y = start_y + (i % rows) * SCORES_ROW_HEIGHT;

		if (!Cg_DrawScore(x, y, s)) {
			i--;
		}
	}
}

/**
 * @brief
 */
void Cg_DrawScores(const player_state_t *ps) {

	if (!ps->stats[STAT_SCORES]) {
		return;
	}

	if (!cg_score_state.num_scores) {
		return;
	}

	const r_pixel_t start_y = Cg_DrawScoresHeader();

	if (cg_state.ctf) {
		Cg_DrawTeamScores(start_y);
	} else if (cg_state.teams) {
		Cg_DrawTeamScores(start_y);
	} else {
		Cg_DrawDmScores(start_y);
	}
}
