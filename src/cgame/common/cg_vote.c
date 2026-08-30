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
#include "bg_vote.h"

static struct {
  ParseConfigString ParseConfigString;
} previous;

/**
 * @brief The tail of the `Cg_ListVoteTypes` hook: the common votes.
 */
static const vote_type_t *Cg_ListVoteTypes_Common(size_t *count) {
  *count = lengthof(vote_types_common);
  return vote_types_common;
}

ListVoteTypes Cg_ListVoteTypes = Cg_ListVoteTypes_Common;

/**
 * @brief Reads `CS_VOTE` into `cg_state.vote`.
 */
static bool Cg_ParseConfigString_Vote(int32_t index) {

  if (index != CS_VOTE) {
    return previous.ParseConfigString(index);
  }

  const char *s = cgi.ConfigString(index);

  memset(&cg_state.vote, 0, sizeof(cg_state.vote));

  if (!*s) {
    return true;
  }

  char buf[MAX_STRING_CHARS];
  q_strlcpy(buf, s, sizeof(buf));

  // split positionally, since a field may be empty
  char *fields[VOTE_CS_FIELDS] = { NULL };
  size_t count = 0;

  for (char *c = buf; count < VOTE_CS_FIELDS; ) {
    fields[count++] = c;

    char *sep = strchr(c, '\\');
    if (!sep) {
      break;
    }
    *sep = '\0';
    c = sep + 1;
  }

  if (count != VOTE_CS_FIELDS) {
    Cg_Warn("Invalid vote: %s\n", s);
    return true;
  }

  cg_state.vote.active = true;
  q_strlcpy(cg_state.vote.type, fields[VOTE_CS_TYPE], sizeof(cg_state.vote.type));
  q_strlcpy(cg_state.vote.arg, fields[VOTE_CS_ARG], sizeof(cg_state.vote.arg));
  cg_state.vote.yes = (int32_t) strtol(fields[VOTE_CS_YES], NULL, 10);
  cg_state.vote.no = (int32_t) strtol(fields[VOTE_CS_NO], NULL, 10);
  cg_state.vote.eligible = (int32_t) strtol(fields[VOTE_CS_ELIGIBLE], NULL, 10);
  cg_state.vote.deadline = (uint32_t) strtoul(fields[VOTE_CS_DEADLINE], NULL, 10);
  q_strlcpy(cg_state.vote.initiator, fields[VOTE_CS_INITIATOR], sizeof(cg_state.vote.initiator));

  return true;
}

/**
 * @see cg_vote.h
 */
void Cg_Vote_Draw(void) {

  if (!cg_state.vote.active) {
    return;
  }

  int32_t ch;
  cgi.BindFont("small", NULL, &ch);

  const uint32_t time = cgi.client->time;
  const uint32_t left = cg_state.vote.deadline > time ? (cg_state.vote.deadline - time) / 1000 : 0;

  const char *lines[] = {
    va("%s called a vote: %s%s%s", cg_state.vote.initiator, cg_state.vote.type,
       *cg_state.vote.arg ? " " : "", cg_state.vote.arg),
    va("Yes %d  No %d  of %d  %us", cg_state.vote.yes, cg_state.vote.no, cg_state.vote.eligible, left),
  };

  int32_t y = cgi.context->h / 2 + 4 * ch;

  for (size_t i = 0; i < lengthof(lines); i++) {
    const int32_t x = (cgi.context->w - cgi.StringWidth(lines[i])) / 2;
    cgi.Draw2DString(x, y, lines[i], i ? color_white : color_green);
    y += ch;
  }

  cgi.BindFont(NULL, NULL, NULL);
}

/**
 * @see cg_vote.h
 */
void Cg_Vote_Cast(bool yes) {
  cgi.Cbuf(va("vote %s\n", yes ? "yes" : "no"));
}

/**
 * @see cg_vote.h
 */
void Cg_Vote_Call(const char *type, const char *arg) {
  cgi.Cbuf(va("vote %s \"%s\"\n", type, arg));
}

/**
 * @brief Installs voting over the hooks it needs, once per module image.
 */
void Cg_Vote_Init(void) {
  static bool installed;

  cgi.AddCmd("vote", NULL, CMD_CGAME, "Call a vote, or cast one: vote <type> [argument], vote yes, vote no");

  if (installed) {
    return;
  }

  previous.ParseConfigString = Cg_ParseConfigString;
  Cg_ParseConfigString = Cg_ParseConfigString_Vote;


  installed = true;
}
