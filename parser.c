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
	int pipe;

	buffer *buf;
	int nchunks;
	char *chunks[MAX_CHUNKS];
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

static void parse_start(parser_state *st)
{
	st->quote = 0;
	st->escape = 0;
	st->pipe = 0;

	st->buf = buffer_alloc(16);
	st->nchunks = 0;
}

static void parse_char(char c, parser_state *st)
{
	if(st->nchunks == MAX_CHUNKS) return;

	if(!st->pipe) {
		if(st->escape) {
			st->escape = 0;
		} else {
			if(isspace(c) && !st->quote) {
				if(st->buf->next) {
					st->chunks[st->nchunks++] = buffer_dup2str(st->buf);
					st->buf->next = 0;
					return;
				}
			}

			if((c == '"' || c == '\'') &&
			   (!st->quote || st->quote == c)) {
				st->quote = (st->quote ? 0 : c);
				return;
			}

			if(c == '\\') {
				st->escape = 1;
				return;
			}

			if(c == '>' || c == '|') st->pipe = 0;
		}
	}

	buffer_append(st->buf, c);
}

static parsed_line *parse_end(parser_state *st)
{
	parsed_line *l;
	int i;

	if(st->buf->next) st->chunks[st->nchunks++] = buffer_dup2str(st->buf);
	buffer_free(st->buf);

	if(!(l = calloc(1, sizeof(parsed_line)))) err_system();
	if(!(l->chunks = calloc(st->nchunks, sizeof(char *)))) err_system();

	l->nchunks = st->nchunks;
	for(i = 0; i < st->nchunks; i++) l->chunks[i] = st->chunks[i];

	return l;
}

parsed_line *parse_buffer(buffer *b)
{
	parser_state st;
	int i;

	parse_start(&st);
	for(i = 0; i < b->next; i++) parse_char(b->buf[i], &st);
	return parse_end(&st);
}

void free_parsed_line(parsed_line *l)
{
	free(l->chunks);
	free(l);
}
