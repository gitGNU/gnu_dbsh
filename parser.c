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
#include "parser.h"


buffer_type get_buffer_type(sql_buffer *b)
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

