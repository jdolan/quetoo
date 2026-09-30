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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"
#include "bg_vote.h"

static struct {
  ParseConfigString ParseConfigString;
} previous;

/**
 * @brief The tail of the `Cg_ListVoteTypes` hook: the common votes.
 */
static const VoteType *Cg_ListVoteTypes_Common(size_t *count) {
  *count = lengthof(voteTypesCommon);
  return voteTypesCommon;
}

ListVoteTypes Cg_ListVoteTypes = Cg_ListVoteTypes_Common;

/**
 * @brief Reads `CS_VOTE` into `cgameState.vote`.
 */
static bool Cg_ParseConfigString_Vote(int32_t index) {

  if (index != CS_VOTE) {
    return previous.ParseConfigString(index);
  }

  const char *s = cgi.ConfigString(index);

  memset(&cgameState.vote, 0, sizeof(cgameState.vote));

  if (!*s) {
    return true;
  }

  char buf[MAX_STRING_CHARS];
  Str_Copy(buf, s, sizeof(buf));

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

  cgameState.vote.active = true;
  Str_Copy(cgameState.vote.type, fields[VOTE_CS_TYPE], sizeof(cgameState.vote.type));
  Str_Copy(cgameState.vote.arg, fields[VOTE_CS_ARG], sizeof(cgameState.vote.arg));
  cgameState.vote.yes = (int32_t) strtol(fields[VOTE_CS_YES], NULL, 10);
  cgameState.vote.no = (int32_t) strtol(fields[VOTE_CS_NO], NULL, 10);
  cgameState.vote.eligible = (int32_t) strtol(fields[VOTE_CS_ELIGIBLE], NULL, 10);
  cgameState.vote.deadline = (uint32_t) strtoul(fields[VOTE_CS_DEADLINE], NULL, 10);
  Str_Copy(cgameState.vote.initiator, fields[VOTE_CS_INITIATOR], sizeof(cgameState.vote.initiator));

  return true;
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
