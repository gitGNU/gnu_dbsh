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
#include "err.h"
#include "output.h"
#include "parser.h"
#include "results.h"


static void go(SQLHDBC *connp, buffer *sqlbuf, char action, FILE *stream)
{
	results *res = NULL;

	switch(get_buffer_type(sqlbuf)) {
	case BUFFER_EMPTY:
		// do nothing
		break;
	case BUFFER_COMMAND:
		res = run_command(connp, sqlbuf);
		break;
	case BUFFER_SQL:
		res = execute_query(*connp, sqlbuf->buf, sqlbuf->next);
		break;
	}

	if(res) {
		output_results(res, action, stream);
		results_free(res);
	}
}

static void edit(buffer *sqlbuf)
{
	char *editor;
	char path[1024];
	char cmd[2014];
	int f;

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
		// TODO: resize buffer if necessary
		sqlbuf->next = read(f, sqlbuf->buf, sqlbuf->len - 1);
		close(f);
	}

	unlink(path);
}

static void print(buffer *sqlbuf, FILE *stream)
{
	fwrite(sqlbuf->buf, 1, sqlbuf->next, stream);
	fputc('\n', stream);
}

void run_action(SQLHDBC *connp, buffer *sqlbuf, char action, char *paramstring)
{
	parsed_line *l;
	int nchunks;
	char *pipeline, *p;
	FILE *stream;
	int m;

	pipeline = 0;
	m = 0;

	l = parse_string(paramstring);
	nchunks = l->nchunks;

	if(nchunks) {
		p = l->chunks[nchunks - 1];
		if(*p == '>') {
			if(!(pipeline = malloc(strlen(p) + 5))) err_system();
			m = 1;
			sprintf(pipeline, "cat %s", p);
			nchunks--;
		} else if(*p == '|') {
			pipeline = p + 1;
			nchunks--;
		}
	}

	if(!pipeline) pipeline = getenv("DBSH_DEFAULT_PAGER");

	if(pipeline) {
		stream = popen(pipeline, "w");
		if(!stream) {
			perror("Failed to open pipe");
			return;
		}
		if(m) free(pipeline);
	} else stream = stdout;


	switch(action) {
	case 'e':  // edit
		edit(sqlbuf);
		print(sqlbuf, stdout);
		break;
	case 'l':  // load
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':  // print
		print(sqlbuf, stream);
		break;
	case 'r':
		db_reconnect(connp, nchunks > 0 ? l->chunks[0] : 0);
		break;
	case 's':  // save
		// TODO: save to named buffer
		break;
	default:
		go(connp, sqlbuf, action, stream);
		break;
	}


	if(pipeline) pclose(stream);

	free_parsed_line(l);
}
