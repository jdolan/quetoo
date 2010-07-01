/**
 * @file scripts.c
 * @brief UFO scripts used in client and server
 * @note interpreters for: object, inventory, equipment, name and team, damage
 */

/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "client.h"
#include "scripts.h"

/** @todo remove me if PPC users dont have problems */
#define ALIGN_NOTHING(size)  (size)

/**
 * @brief Parsing function that prints an error message when there is no text in the buffer
 * @sa Com_Parse
 */
const char *Com_EParse (const char **text, const char *errhead, const char *errinfo)
{
	const char *token;

	token = Com_Parse(text);
	if (!*text) {
		if (errinfo)
			Com_Printf("%s \"%s\")\n", errhead, errinfo);
		else
			Com_Printf("%s\n", errhead);

		return NULL;
	}

	return token;
}

/**
 * @brief possible values for parsing functions
 * @sa valueTypes_t
 */
const char *const vt_names[] = {
	"",
	"bool",
	"char",
	"int",
	"int2",
	"float",
	"pos",
	"vector",
	"color",
	"rgba",
	"string",
	"longstring",
	"align",
	"longlines",
};
CASSERT(lengthof(vt_names) == V_NUM_TYPES);

const char *const align_names[] = {
	"ul", "uc", "ur", "cl", "cc", "cr", "ll", "lc", "lr", "ul_rsl", "uc_rsl", "ur_rsl", "cl_rsl", "cc_rsl", "cr_rsl", "ll_rsl", "lc_rsl", "lr_rsl"
};
CASSERT(lengthof(align_names) == ALIGN_LAST);

const char *const longlines_names[] = {
	"wrap", "chop", "prettychop"
};
CASSERT(lengthof(longlines_names) == LONGLINES_LAST);

/** @brief target sizes for buffer */
static const size_t vt_sizes[] = {
	0,	/* V_NULL */
	sizeof(qboolean),	/* V_BOOL */
	sizeof(char),	/* V_CHAR */
	sizeof(int),	/* V_INT */
	2 * sizeof(int),	/* V_INT2 */
	sizeof(float),	/* V_FLOAT */
	sizeof(vec2_t),	/* V_POS */
	sizeof(vec3_t),	/* V_VECTOR */
	sizeof(vec4_t),	/* V_COLOR */
	sizeof(vec4_t),	/* V_RGBA */
	0,	/* V_STRING */
	0,	/* V_LONGSTRING */
	sizeof(byte),	/* V_ALIGN */
	sizeof(byte) 	/* V_LONGLINES */
};
CASSERT(lengthof(vt_sizes) == V_NUM_TYPES);

/** @brief natural align for each targets */
static const size_t vt_aligns[] = {
	0,	/* V_NULL */
	sizeof(qboolean),	/* V_BOOL */
	sizeof(char),	/* V_CHAR */
	sizeof(int),	/* V_INT */
	sizeof(int),	/* V_INT2 */
	sizeof(float),	/* V_FLOAT */
	sizeof(vec_t),	/* V_POS */
	sizeof(vec_t),	/* V_VECTOR */
	sizeof(vec_t),	/* V_COLOR */
	sizeof(vec_t),	/* V_RGBA */
	sizeof(char),	/* V_STRING */
	sizeof(char),	/* V_LONGSTRING */
	sizeof(byte),	/* V_ALIGN */
	sizeof(byte) 	/* V_LONGLINES */
};
CASSERT(lengthof(vt_aligns) == V_NUM_TYPES);

/**
 * @brief Align a memory to use a natural address for the data type we will write
 * @note it speeds up data read, and fix crash on PPC processors
 */
void* Com_AlignPtr (void *memory, valueTypes_t type)
{
	const int align = vt_aligns[type];
	return ALIGN_PTR(memory, align);
}

static char parseErrorMessage[256];

/**
 * Return the last error message
 * @return adresse containe the last message
 */
const char* Com_GetLastParseError (void)
{
	return parseErrorMessage;
}

int Com_EParseValue (void *base, const char *token, valueTypes_t type, int ofs, size_t size)
{
	size_t writtenBytes;
	const resultStatus_t result = Com_ParseValue(base, token, type, ofs, size, &writtenBytes);
	switch (result) {
	case RESULT_ERROR:
		Sys_Error("Com_EParseValue: %s\n", parseErrorMessage);
		break;
	case RESULT_WARNING:
		Com_Printf("Com_EParseValue: %s\n", parseErrorMessage);
		break;
	case RESULT_OK:
		break;
	}
	return writtenBytes;
}

/**
 * Parse a value
 * @param[out] writedByte
 * @return A resultStatus_t value
 * @note instead of , this function separate error message and write byte result
 * @todo better doxygen documentation
 */
int Com_ParseValue (void *base, const char *token, valueTypes_t type, int ofs, size_t size, size_t *writedByte)
{
	byte *b;
	byte num;
	int w;
	resultStatus_t status = RESULT_OK;
	b = (byte *) base + ofs;
	*writedByte = 0;

	switch (type) {
	case V_NULL:
		*writedByte = ALIGN_NOTHING(0);
		break;

	case V_BOOL:
		if (!strcmp(token, "true") || *token == '1')
			*b = qtrue;
		else if (!strcmp(token, "false") || *token == '0')
			*b = qfalse;
		else {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal bool statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(sizeof(qboolean));
		break;

	case V_CHAR:
		*(char *) b = *token;
		*writedByte = ALIGN_NOTHING(sizeof(char));
		break;

	case V_INT:
		if (sscanf(token, "%i", &((int *) b)[0]) != 1) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal int statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(sizeof(int));
		break;

	case V_INT2:
		if (sscanf(token, "%i %i", &((int *) b)[0], &((int *) b)[1]) != 2) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal int2 statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(2 * sizeof(int));
		break;

	case V_FLOAT:
		if (sscanf(token, "%f", &((float *) b)[0]) != 1) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal float statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(sizeof(float));
		break;

	case V_POS:
		if (sscanf(token, "%f %f", &((float *) b)[0], &((float *) b)[1]) != 2) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal pos statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(2 * sizeof(float));
		break;

	case V_VECTOR:
		if (sscanf(token, "%f %f %f", &((float *) b)[0], &((float *) b)[1], &((float *) b)[2]) != 3) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal vector statement '%s'", token);
			return RESULT_ERROR;
		}
		*writedByte = ALIGN_NOTHING(3 * sizeof(float));
		break;

	case V_COLOR:
		{
			float* f = (float *) b;
			if (sscanf(token, "%f %f %f %f", &f[0], &f[1], &f[2], &f[3]) != 4) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal color statement '%s'", token);
				return RESULT_ERROR;
			}
			*writedByte = ALIGN_NOTHING(4 * sizeof(float));
		}
		break;

	case V_RGBA:
		{
			int* i = (int *) b;
			if (sscanf(token, "%i %i %i %i", &i[0], &i[1], &i[2], &i[3]) != 4) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal rgba statement '%s'", token);
				return RESULT_ERROR;
			}
			*writedByte = ALIGN_NOTHING(4 * sizeof(int));
		}
		break;

	case V_STRING:
		Q_strncpyz((char *) b, token, MAX_VAR);
		w = (int)strlen(token) + 1;
		*writedByte = ALIGN_NOTHING(w);
		break;

	case V_LONGSTRING:
		strcpy((char *) b, token);
		w = (int)strlen(token) + 1;
		*writedByte = ALIGN_NOTHING(w);
		break;

	case V_ALIGN:
		for (num = 0; num < ALIGN_LAST; num++)
			if (!strcmp(token, align_names[num]))
				break;
		if (num == ALIGN_LAST)
			*b = 0;
		else
			*b = num;
		*writedByte = ALIGN_NOTHING(sizeof(byte));
		break;

	case V_LONGLINES:
		for (num = 0; num < LONGLINES_LAST; num++)
			if (!strcmp(token, longlines_names[num]))
				break;
		if (num == LONGLINES_LAST)
			*b = 0;
		else
			*b = num;
		*writedByte = ALIGN_NOTHING(sizeof(byte));
		break;

	default:
		snprintf(parseErrorMessage, sizeof(parseErrorMessage), "unknown value type '%s'", token);
		return RESULT_ERROR;
	}
	return status;
}

/**
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] set The data which should be parsed
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @sa Com_ValueToStr
 * @note The offset is most likely given by the offsetof macro
 */
int Com_SetValue (void *base, const void *set, valueTypes_t type, int ofs, size_t size)
{
	byte *b;

	b = (byte *) base + ofs;

	if (size && size < vt_sizes[type])
		Sys_Error("Size mismatch given for type %i", type);

	switch (type) {
	case V_NULL:
		return ALIGN_NOTHING(0);

	case V_BOOL:
		if (*(const qboolean *) set)
			*(qboolean *)b = true;
		else
			*(qboolean *)b = false;
		return ALIGN_NOTHING(sizeof(qboolean));

	case V_CHAR:
		*(char *) b = *(const char *) set;
		return ALIGN_NOTHING(sizeof(char));

	case V_INT:
		*(int *) b = *(const int *) set;
		return ALIGN_NOTHING(sizeof(int));

	case V_INT2:
		((int *) b)[0] = ((const int *) set)[0];
		((int *) b)[1] = ((const int *) set)[1];
		return ALIGN_NOTHING(2 * sizeof(int));

	case V_FLOAT:
		*(float *) b = *(const float *) set;
		return ALIGN_NOTHING(sizeof(float));

	case V_POS:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		return ALIGN_NOTHING(2 * sizeof(float));

	case V_VECTOR:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		((float *) b)[2] = ((const float *) set)[2];
		return ALIGN_NOTHING(3 * sizeof(float));

	case V_COLOR:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		((float *) b)[2] = ((const float *) set)[2];
		((float *) b)[3] = ((const float *) set)[3];
		return ALIGN_NOTHING(4 * sizeof(float));

	case V_RGBA:
		((int *) b)[0] = ((const int *) set)[0];
		((int *) b)[1] = ((const int *) set)[1];
		((int *) b)[2] = ((const int *) set)[2];
		((int *) b)[3] = ((const int *) set)[3];
		return ALIGN_NOTHING(4 * sizeof(int));

	case V_STRING:
		strncpy((char *) b, (const char *) set, 64);
		return (int)strlen((const char *) set) + 1;

	case V_LONGSTRING:
		strcpy((char *) b, (const char *) set);
		return (int)strlen((const char *) set) + 1;

	case V_ALIGN:
		*b = *(const byte *) set;
		return ALIGN_NOTHING(1);

	default:
		Sys_Error("Com_SetValue: unknown value type\n");
	}
}

/**
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @sa Com_SetValue
 * @return char pointer with translated data type value
 */
const char *Com_ValueToStr (const void *base, const valueTypes_t type, const int ofs)
{
	static char valuestr[64];
	const byte *b;

	b = (const byte *) base + ofs;

	switch (type) {
	case V_NULL:
		return 0;

	case V_BOOL:
		if (*b)
			return "true";
		else
			return "false";

	case V_CHAR:
		return (const char *) b;
		break;

	case V_INT:
		snprintf(valuestr, sizeof(valuestr), "%i", *(const int *) b);
		return valuestr;

	case V_INT2:
		snprintf(valuestr, sizeof(valuestr), "%i %i", ((const int *) b)[0], ((const int *) b)[1]);
		return valuestr;

	case V_FLOAT:
		snprintf(valuestr, sizeof(valuestr), "%.2f", *(const float *) b);
		return valuestr;

	case V_POS:
		snprintf(valuestr, sizeof(valuestr), "%.2f %.2f", ((const float *) b)[0], ((const float *) b)[1]);
		return valuestr;

	case V_VECTOR:
		snprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f", ((const float *) b)[0], ((const float *) b)[1], ((const float *) b)[2]);
		return valuestr;

	case V_COLOR:
		snprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f %.2f", ((const float *) b)[0], ((const float *) b)[1], ((const float *) b)[2], ((const float *) b)[3]);
		return valuestr;

	case V_RGBA:
		snprintf(valuestr, sizeof(valuestr), "%3i %3i %3i %3i", ((const int *) b)[0], ((const int *) b)[1], ((const int *) b)[2], ((const int *) b)[3]);
		return valuestr;

	case V_STRING:
	case V_LONGSTRING:
		if (b == NULL)
			return "(null)";
		else
			return (const char *) b;

	case V_ALIGN:
		strncpy(valuestr, align_names[*(const align_t *)b], sizeof(valuestr));
		return valuestr;

	default:
		Sys_Error("Com_ValueToStr: unknown value type %i\n", type);
	}
}


/**
 * @brief Safe (null terminating) vsnprintf implementation
 */
int Q_vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
	int len;

#if defined(_WIN32)
	len = _vsnprintf(str, size, format, ap);
	str[size - 1] = '\0';
#else
	len = vsnprintf(str, size, format, ap);
#endif

	return len;
}


/**
 * @brief Safe strncpy that ensures a trailing zero
 * @param dest Destination pointer
 * @param src Source pointer
 * @param destsize Size of destination buffer (this should be a sizeof size due to portability)
 */
void Q_strncpyz (char *dest, const char *src, size_t destsize)
{
	/* space for \0 terminating */
	while (*src && destsize - 1) {
		*dest++ = *src++;
		destsize--;
	}
	/* terminate the string */
	*dest = '\0';
}

/**
 * @brief Safely (without overflowing the destination buffer) concatenates two strings.
 * @param[in] dest the destination string.
 * @param[in] src the source string.
 * @param[in] destsize the total size of the destination buffer.
 * @return pointer destination string.
 * never goes past bounds or leaves without a terminating 0
 */
void Q_strcat (char *dest, const char *src, size_t destsize)
{
	size_t dest_length;
	dest_length = strlen(dest);
	if (dest_length >= destsize)
		Sys_Error("Q_strcat: already overflowed");
	Q_strncpyz(dest + dest_length, src, destsize - dest_length);
}

void FS_SkipBlock (const char **text)
{
	const char *token;
	int depth;

	depth = 1;

	do {
		token = Com_Parse(text);
		if (*token == '{')
			depth++;
		else if (*token == '}')
			depth--;
	} while (depth && *text);
}

#define MACRO_CVAR_ID_LENGTH 6
/**
 * @brief Expands strings with cvar values that are dereferenced by a '*cvar'
 * @note There is an overflow check for cvars that also contain a '*cvar'
 * @sa Cmd_TokenizeString
 * @sa MN_GetReferenceString
 */
const char *Com_MacroExpandString (const char *text)
{
	int i, j, count, len;
	qboolean inquote;
	const char *scan;
	static char expanded[MAX_STRING_CHARS];
	const char *token, *start, *cvarvalue;
	char *pos;

	inquote = qfalse;
	scan = text;
	if (!text || !*text)
		return NULL;

	len = strlen(scan);
	if (len >= MAX_STRING_CHARS) {
		Com_Printf("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;
	memset(expanded, 0, sizeof(expanded));
	pos = expanded;

	/* also the \0 */
	assert(scan[len] == '\0');
	for (i = 0; i <= len; i++) {
		if (scan[i] == '"')
			inquote ^= 1;
		/* don't expand inside quotes */
		if (inquote || strncmp(&scan[i], "*cvar ", MACRO_CVAR_ID_LENGTH)) {
			*pos++ = scan[i];
			continue;
		}

		/* scan out the complete macro and only parse the cvar name */
		start = &scan[i + MACRO_CVAR_ID_LENGTH];
		token = Com_Parse(&start);
		if (!start)
			continue;

		/* skip the macro and the cvar name in the next loop */
		i += MACRO_CVAR_ID_LENGTH;
		i += strlen(token);
		i--;

		/* get the cvar value */
		cvarvalue = Cvar_GetString(token);
		if (!cvarvalue) {
			Com_Printf("Could not get cvar value for cvar: %s\n", token);
			return NULL;
		}
		j = strlen(cvarvalue);
		if (strlen(pos) + j >= MAX_STRING_CHARS) {
			Com_Printf("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		/* copy the cvar value into the target buffer */
		/* check for overflow is already done - so MAX_STRING_CHARS won't hurt here */
		Q_strncpyz(pos, cvarvalue, j + 1);
		pos += j;

		if (++count == 100) {
			Com_Printf("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote) {
		Com_Printf("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	if (count)
		return expanded;
	else
		return NULL;
}

void R_FontTextSize(const char *fontID, const char *text, int maxWidth, int chop, int *width, int *height, int *lines, qboolean *isTruncated)
{

}
