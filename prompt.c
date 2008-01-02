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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sql.h>

#include "common.h"
#include "buffer.h"
#include "db.h"


#define MAX_LEN 64


static int get_lnum(buffer *buf)
{
	int i, n = 1;

	for(i = 0; i < buf->next; i++) if(buf->buf[i] == '\n') n++;

	return n;
}

const char *prompt_render(SQLHDBC conn, buffer *buf)
{
	static char prompt[MAX_LEN];

	const char *s;
	int i;

	s = getenv("DBSH_PROMPT");

	i = 0;
	for(; *s; s++) {
		switch(*s) {
		case 'd':
			i += db_info(conn, SQL_DATA_SOURCE_NAME, prompt + i, MAX_LEN - i);
			break;
		case 'l':
			i += snprintf(prompt + i, MAX_LEN - i, "%d", get_lnum(buf));
			break;
		default:
			prompt[i++] = *s;
		}

		if(i >= MAX_LEN - 1) break;
	}

	prompt[i] = 0;

	return prompt;
}

