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

#include <config.h>

#if defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDIT) || defined(HAVE_LIBEDITLINE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_LIBREADLINE)
#include <readline/readline.h>
#include <readline/history.h>
#elif defined(HAVE_LIBEDIT)
#include <editline/readline.h>
#elif defined(HAVE_LIBEDITLINE)
#include <editline.h>
#endif

#include "common.h"
#include "buffer.h"
#include "err.h"
#include "rc.h"

#ifndef HAVE_LIBEDITLINE
static const char *get_history_filename()
{
	static char *filename = 0;

	const char *rc_dir;
	int l;

	if(!filename) {
		if((rc_dir = get_rc_dir())) {
			l = strlen(rc_dir) + 9;
			if(!(filename = malloc(l))) err_system();
			snprintf(filename, l, "%s/history", rc_dir);
		}
	}

	return filename;
}
#endif

char *rl_readline(const char *prompt)
{
	return readline(prompt);
}

void rl_history_start()
{
#ifndef HAVE_LIBEDITLINE

	const char *hf;

	using_history();
	hf = get_history_filename();
	if(hf) read_history(hf);

#endif
}

void rl_history_add(buffer *buf, const char *action_line)
{
	int len, i;
	char *actionchars, *histentry, *p;

#ifndef HAVE_LIBEDITLINE
	HIST_ENTRY *prev;
	const char *hf;
#endif

	actionchars = getenv("DBSH_ACTION_CHARS");

	len = buf->next + strlen(action_line) + 1;
	for(i = 0; i < buf->next; i++) if(strchr(actionchars, buf->buf[i])) len++;

	histentry = malloc(len);

	if(histentry) {
		p = histentry;
		for(i = 0; i < buf->next; i++) {
			if(strchr(actionchars, buf->buf[i])) *p++ = actionchars[0];
			*p++ = buf->buf[i];
		}
		strcpy(p, action_line);

#ifdef HAVE_LIBEDITLINE

		add_history(histentry);
#else
		using_history();

		prev = previous_history();
		if(!prev || strcmp(prev->line, histentry)) add_history(histentry);

		free(histentry);

		using_history();

		// This can be removed once this client is stable enough to trust
		hf = get_history_filename();
		if(hf) write_history(hf);
#endif
	}
}

void rl_history_end()
{
#ifndef HAVE_LIBEDITLINE

	const char *hf;
	hf = get_history_filename();
	if(hf) write_history(hf);

#endif
}

#else

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "buffer.h"
#include "err.h"

char *rl_readline(const char *prompt)
{
	char *buf;
	fputs(prompt, stdout);
	if(!(buf = (char *) malloc(1024))) err_system();
	fgets(buf, 1024, stdin);
	return buf;
}

void rl_history_start()
{
}

void rl_history_add(buffer *buf, const char *action_line)
{
}

void rl_history_end()
{
}

#endif
