/*
    dbsh - text-based ODBC client
    Copyright (C) 2007 Ben Spencer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "err.h"
#include "parser.h"


#define MAX_CHUNKS 16

typedef struct {
	char quote;
	int escape;
} parser_state;


buffer_type get_buffer_type(buffer *b)
{
	int i;

	for(i = 0; i < b->next; i++) {
		if(!isspace(b->buf[i])) {
			if(strchr(getenv("DBSH_COMMAND_CHARS"), b->buf[i]))
				return BUFFER_COMMAND;
			break;
		}
	}

	return BUFFER_SQL;
}

static char parse_char(char c, parser_state *st)
{
	if(st->escape) {
		st->escape = 0;
	} else {
		if(isspace(c) && !st->quote) return 1;

		if((c == '"' || c == '\'') && (!st->quote || st->quote == c)) {
				st->quote = (st->quote ? 0 : c);
				return 0;
		}

		if(c == '\\') {
			st->escape = 1;
			return 0;
		}
	}

	return c;
}

parsed_line *parse_buffer(buffer *b)
{
	parser_state st = {0};
	buffer *t;
	int nchunks;
	char *chunks[MAX_CHUNKS];
	int i;
	char c;
	parsed_line *l;

	t = buffer_alloc(16);
	nchunks = 0;

	for(i = 0; i < b->next; i++) {
		c = parse_char(b->buf[i], &st);
		switch(c) {
		case 0:
			break;
		case 1:
			if(t->next) {
				chunks[nchunks++] = buffer_dup2str(t);
				t->next = 0;
			}
			break;
		default:
			buffer_append(t, c);
		}

		if(nchunks == MAX_CHUNKS) break;
	}

	if(t->next) chunks[nchunks++] = buffer_dup2str(t);
	buffer_free(t);

	if(!(l = calloc(1, sizeof(parsed_line)))) err_system();
	if(!(l->chunks = calloc(nchunks, sizeof(char *)))) err_system();

	l->nchunks = nchunks;
	for(i = 0; i < nchunks; i++) l->chunks[i] = chunks[i];

	return l;
}

void free_parsed_line(parsed_line *l)
{
	free(l->chunks);
	free(l);
}
