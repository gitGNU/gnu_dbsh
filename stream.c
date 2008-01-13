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

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "common.h"
#include "err.h"
#include "stream.h"


stream *stream_create(FILE *f)
{
	stream *s;

	if(!(s = calloc(1, sizeof(stream)))) err_system();
	s->f = f;
	return s;
}

void stream_reset(stream *s)
{
	if(!mbsinit(&s->ps)) {
		stream_putwc(s, 0);
	}
}

void stream_puts(stream *s, const char *string)
{
	stream_reset(s);
	fputs(string, s->f);
}

void stream_write(stream *s, const char *buf, size_t len)
{
	stream_reset(s);
	fwrite(buf, sizeof(char), len, s->f);
}

void stream_printf(stream *s, const char *fmt, ...)
{
	va_list ap;
	int ret;

	stream_reset(s);

	va_start(ap, fmt);
	ret = vfprintf(s->f, fmt, ap);
	va_end(ap);
}

void stream_putwc(stream *s, wchar_t wc)
{
	char mb[MB_LEN_MAX];
	size_t l;

	if((l = wcrtomb(mb, wc, &s->ps)) == -1) err_system();

	fwrite(mb, sizeof(char), l, s->f);
}

void stream_putws(stream *s, const wchar_t *wcs)
{
	mbstate_t psc;
	size_t l;
	char *mbs;

	memcpy(&psc, &s->ps, sizeof(psc));

	l = wcsrtombs(0, &wcs, 0, &psc);
	if(l == -1) err_system();

	if(!(mbs = calloc(l + 1, sizeof(char)))) err_system();
	wcsrtombs(mbs, &wcs, l, &s->ps);

	fputs(mbs, s->f);
	free(mbs);
}

void stream_space(stream *s)
{
	stream_putwc(s, L' ');
}

void stream_newline(stream *s)
{
	stream_putwc(s, L'\n');
}
