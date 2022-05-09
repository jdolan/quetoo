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
	size_t num_scores;
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
		if (cg_state.ctf || cg_state.num_teams) {
			cg_score_state.num_scores -= MAX_TEAMS;
		}
	}

	qsort(cg_score_state.scores, cg_score_state.num_scores, sizeof(g_score_t), Cg_ParseScores_Compare);
}

/**
 * @brief Returns the vertical screen coordinate where scores should be drawn.
 */
static GLint Cg_DrawScoresHeader(void) {
	GLint cw, ch, x, y;

	cgi.BindFont("medium", &cw, &ch);

	y = 64 - ch - 4;

	const char *map_name = cgi.ConfigString(CS_NAME);
	const GLint sw = cgi.StringWidth(map_name);

	// map title
	x = cgi.context->width / 2 - sw / 2;
	cgi.Draw2DString(x, y, map_name, color_white);

	y += ch;

	// team names and scores
	if (cg_state.num_teams) {
		cgi.BindFont("small", &cw, &ch);

		g_score_t *score = &cg_score_state.scores[cg_score_state.num_scores];

		// start from center
		x = cgi.context->width / 2;
		x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);
		x += SCORES_ICON_WIDTH;

		const cg_team_info_t *team = cg_state.teams;
		for (int32_t i = 0; i < cg_state.num_teams; i++, score++, team++) {

			if (cg_state.ctf) {
				cgi.Draw2DString(x, y, va("%s^7 %d captures", team->name, score->captures), team->color);
			} else {
				cgi.Draw2DString(x, y, va("%s^7 %d frags", team->name, score->score), team->color);
			}

			x += SCORES_COL_WIDTH;
		}

		y += ch;
	}

	return y;
}

/**
 * @brief
 */
static _Bool Cg_DrawScore(GLint x, GLint y, const g_score_t *s) {
	GLint cw, ch;

	const cg_client_info_t *info = &cg_state.clients[s->client];

	// icon
	cgi.Draw2DImage(x + 1, y + 1, SCORES_ICON_WIDTH - 2, SCORES_ICON_WIDTH - 2, info->icon, color_white);

	// flag carrier icon
	if (cg_state.ctf && (s->flags & SCORE_CTF_FLAG)) {
		const int32_t team = s->team;
		const r_image_t *flag = cgi.LoadImage(va("pics/i_flag%d", team), IT_PIC);
		cgi.Draw2DImage(x + 1, y + 1, SCORES_ICON_WIDTH * 0.3f, SCORES_ICON_WIDTH * .3f, flag, color_white);
	}

	x += SCORES_ICON_WIDTH;

	// background
	const float fa = s->client == cgi.client->client_num ? 0.3 : 0.15;
	const GLint fw = SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 1;
	const GLint fh = SCORES_ROW_HEIGHT - 1;

	if (s->color != 0) {
		color_t c = ColorHSV(s->color, 1.0, 1.0);
		c.a = fa;

		cgi.Draw2DFill(x, y, fw, fh, c);
	}

	cgi.BindFont("small", &cw, &ch);

	// name
	cgi.Draw2DString(x, y, info->name, color_white);

	// ping
	{
		const GLint px = x + SCORES_COL_WIDTH - SCORES_ICON_WIDTH - 6 * cw;
		cgi.Draw2DString(px, y, va("%3dms", s->ping), color_white);
		y += ch;
	}

	// spectating
	if (s->flags & SCORE_SPECTATOR) {
		cgi.Draw2DString(x, y, "spectating", color_white);
		return true;
	}

	// frags
	cgi.Draw2DString(x, y, va("%d frags", s->score), color_white);

	// deaths
	char *deaths = va("%d deaths ", s->deaths);
	cgi.Draw2DString(x + fw - cgi.StringWidth(deaths), y, deaths, color_white);
	y += ch;

	// ready/not ready
	if (cg_state.match) {
		if (s->flags & SCORE_NOT_READY) {
			cgi.Draw2DString(x + fw - cgi.StringWidth("not ready "), y, "not ready", color_white);
		}
	}

	// captures
	if (!cg_state.ctf) {
		return true;
	}

	cgi.Draw2DString(x, y, va("%d captures", s->captures), color_white);
	return true;
}

/**
 * @brief
 */
static void Cg_DrawTeamScores(const GLint start_y) {

	size_t rows = (cgi.context->height - (2 * start_y)) / SCORES_ROW_HEIGHT;
	rows = rows < 3 ? 3 : rows;

	GLint x = cgi.context->width / 2;
	x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);

	GLint y = start_y;

	for (int32_t t = 0; t < cg_state.num_teams; t++, x += SCORES_COL_WIDTH, y = start_y) {
		for (size_t i = 0; i < cg_score_state.num_scores; i++) {
			const g_score_t *s = &cg_score_state.scores[i];

			if (s->team != t + 1) {
				continue;
			}

			if (i == rows) {
				break;
			}

			if (Cg_DrawScore(x, y, s)) {
				y += SCORES_ROW_HEIGHT;
			}
		}
	}

	x = cgi.context->width / 2;
	x -= SCORES_COL_WIDTH * (cg_state.num_teams / 2.0);
	x -= SCORES_COL_WIDTH * 2.0;
	y = start_y;

	int32_t j = 0;
	for (size_t i = 0; i < cg_score_state.num_scores; i++) {
		const g_score_t *s = &cg_score_state.scores[i];

		if (!(s->flags & SCORE_SPECTATOR)) {
			continue;
		}

		if (i == rows) {
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
static void Cg_DrawDmScores(const GLint start_y) {

	size_t rows = (cgi.context->height - (2 * start_y)) / SCORES_ROW_HEIGHT;
	rows = rows < 3 ? 3 : rows;

	const size_t cols = (rows < cg_score_state.num_scores) ? 2 : 1;
	const size_t width = cols * SCORES_COL_WIDTH;

	const g_score_t *s = cg_score_state.scores;
	for (size_t i = 0; i < cg_score_state.num_scores; i++, s++) {

		if (i == (cols * rows)) { // screen is full
			break;
		}

		const size_t col = i / rows;

		const GLint x = (GLint) (cgi.context->width / 2 - width / 2 + col * SCORES_COL_WIDTH);
		const GLint y = (GLint) (start_y + (i % rows) * SCORES_ROW_HEIGHT);

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

	const GLint start_y = Cg_DrawScoresHeader();

	if (cg_state.num_teams) {
		Cg_DrawTeamScores(start_y);
	} else {
		Cg_DrawDmScores(start_y);
	}
}
