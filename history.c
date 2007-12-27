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

#include <stdlib.h>
#include <string.h>
#include <readline/history.h>

#include "common.h"
#include "buffer.h"


static const char *get_history_filename()
{
	static char filename[1024];
	char *home;

	home = getenv("HOME");

	if(home) {
		snprintf(filename, 1024, "%s/.%s_history", home, PACKAGE);
		return filename;
	}

	return 0;
}

void history_start()
{
	using_history();
	read_history(get_history_filename());
}

void history_add(sql_buffer *buf, const char *action_line)
{
	char *histentry;
	HIST_ENTRY *prev;

	histentry = malloc(buf->next + strlen(action_line) + 1);

	if(histentry) {
		memcpy(histentry, buf->buf, buf->next);
		strcpy(histentry + buf->next, action_line);

		using_history();

		prev = previous_history();
		if(!prev || strcmp(prev->line, histentry)) add_history(histentry);

		free(histentry);

		using_history();

		// This can be removed once this client is stable enough to trust
		write_history(get_history_filename());
	}
}

void history_end()
{
	write_history(get_history_filename());
}
