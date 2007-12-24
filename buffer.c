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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "err.h"

sql_buffer *buffer_alloc(size_t len)
{
	sql_buffer *b;

	if(!(b = malloc(sizeof(sql_buffer)))) err_system();
	if(!(b->buf = malloc(len))) err_system();

	b->len  = len;
	b->next = 0;

	return b;
}

void buffer_append(sql_buffer *buf, char c)
{
	if(buf->next >= buf->len) {
		buf->len *= 2;
		if(!(buf->buf = realloc(buf->buf, buf->len))) err_system();
	}

	buf->buf[buf->next++] = c;
}

void buffer_set(sql_buffer *buf, const char *s)
{
	int l;

	l = strlen(s);

	if(l + 1 > buf->len) {
		buf->len = l + 1;
		if(!(buf->buf = realloc(buf->buf, buf->len))) err_system();
	}

	strcpy(buf->buf, s);
}

void buffer_free(sql_buffer *b)
{
	free(b->buf);
	free(b);
}
