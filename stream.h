/*
    dbsh - text-based ODBC client
    Copyright (C) 2007, 2008 Ben Spencer

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

#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>
#include <wchar.h>

stream *stream_create(FILE *);
void stream_reset(stream *);
void stream_puts(stream *, const char *);
void stream_write(stream *, const char *, size_t);
void stream_printf(stream *, const char *, ...) __attribute__ ((format(printf, 2, 3)));
void stream_putwc(stream *, wchar_t);
void stream_putws(stream *, const wchar_t *);
void stream_space(stream *);
void stream_newline(stream *);

#endif
