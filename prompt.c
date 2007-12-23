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

#include <sql.h>

#include "common.h"
#include "buffer.h"
#include "db.h"


static int get_lnum(sql_buffer *buf)
{
	int i, n = 1;

	for(i = 0; i < buf->next; i++) if(buf->buf[i] == '\n') n++;

	return n;
}

const char *prompt_render(SQLHDBC conn, sql_buffer *buf)
{
	static char prompt[256];

	const char *tpl;
	char *p;
	int i, l;
	char info[64];

	tpl = getenv("DBSH_PROMPT");
	p = prompt;

	// TODO: check for overflow

	l = strlen(tpl);
	for(i = 0; i < l; i++) {
		switch(tpl[i]) {
		case 'd':
			db_info(conn, SQL_DATA_SOURCE_NAME, info, 64);
			strcpy(p, info);
			p += strlen(info);
			break;
		case 'l':
			p += sprintf(p, "%d", get_lnum(buf));
			break;
		default:
			*p++ = tpl[i];
		}
	}

	return prompt;
}

