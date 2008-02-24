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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "command.h"
#include "db.h"
#include "err.h"
#include "output.h"
#include "parser.h"
#include "results.h"
#include "stream.h"


static void go(buffer *sqlbuf, char action, parsed_line *params, stream *stream)
{
	results *res = NULL;

	switch(get_buffer_type(sqlbuf)) {
	case BUFFER_EMPTY:
		// do nothing
		break;
	case BUFFER_COMMAND:
		res = run_command(sqlbuf);
		break;
	case BUFFER_SQL:
		res = execute_query(sqlbuf->buf, sqlbuf->next, params);
		break;
	}

	if(res) {
		output_results(res, action, stream);
		res_free(res);
	}
}

static void edit(buffer *sqlbuf)
{
	char *editor;
	char path[1024];
	char cmd[2014];
	int f;
	size_t n;

	editor = getenv("EDITOR");
	if(!editor) editor = getenv("VISUAL");
	if(!editor) editor = "vi";

	// FIXME: make this safe
	snprintf(path, 1024, "/tmp/%s-edit.%d", PACKAGE, getpid());

	f = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if(f == -1) {
		perror("Failed to open temporary file");
		return;
	}
	write(f, sqlbuf->buf, sqlbuf->next);
	close(f);

	snprintf(cmd, 1024, "%s %s", editor, path);
	system(cmd);

	f = open(path, O_RDONLY);
	if(f != -1) {
		sqlbuf->next = 0;

		do {
			n = read(f, sqlbuf->buf + sqlbuf->next, sqlbuf->len - sqlbuf->next);
			sqlbuf->next += n;
			if(sqlbuf->next == sqlbuf->len) buffer_realloc(sqlbuf, sqlbuf->len * 2);
		} while(n);

		close(f);
	}

	unlink(path);
}

static void print(buffer *sqlbuf, stream *stream)
{
	stream_write(stream, sqlbuf->buf, sqlbuf->next);
	stream_putwc(stream, L'\n');
}

void run_action(buffer *sqlbuf, char action, char *paramstring)
{
	parsed_line *l;
	char *pipeline;
	FILE *f;
	stream *stream;
	int m;

	pipeline = 0;
	m = 0;

	l = parse_string(paramstring);

	if(l->pipeline) {
		if(*l->pipeline == '>') {
			if(!(pipeline = malloc(strlen(l->pipeline) + 5))) err_system();
			m = 1;
			sprintf(pipeline, "cat %s", l->pipeline);
		} else if(*l->pipeline == '|') {
			pipeline = l->pipeline + 1;
		}
	}

	if(!pipeline) pipeline = getenv("DBSH_DEFAULT_PAGER");

	if(pipeline) {
		f = popen(pipeline, "w");
		if(m) free(pipeline);
		if(!f) {
			perror("Failed to open pipe");
			return;
		}
	} else f = stdout;

	stream = stream_create(f);


	switch(action) {
	case 'e':  // edit
		edit(sqlbuf);
		print(sqlbuf, stream);
		break;
	case 'l':  // load
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':  // print
		print(sqlbuf, stream);
		break;
	case 'r':
		db_reconnect();
		break;
	case 's':  // save
		// TODO: save to named buffer
		break;
	default:
		go(sqlbuf, action, l, stream);
		break;
	}


	stream_reset(stream);
	if(pipeline) pclose(f);
	free(stream);

	free_parsed_line(l);
}
