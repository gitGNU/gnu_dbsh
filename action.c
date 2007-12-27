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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "command.h"
#include "output.h"
#include "results.h"


static void go(SQLHDBC *connp, sql_buffer *sqlbuf, char action, char *paramstring)
{
	results *res = NULL;
	FILE *stream;
	int stype;
	int i;
	char *default_pager;

	// TODO: proper parsing

	if(strchr(getenv("DBSH_COMMAND_CHARS"), sqlbuf->buf[0])) {
		res = run_command(connp, sqlbuf->buf, sqlbuf->next);
	} else {
		res = execute_query(*connp, sqlbuf->buf, sqlbuf->next);
	}

	if(res) {
		stream = stdout;
		stype = 0;

		for(i = 0; i < strlen(paramstring); i++) {
			if(paramstring[i] == '>') {
				char *filename;
				filename = strtok(paramstring + i + 1, " ");
				stream = fopen(filename, "w");
				if(!stream) {
					perror("Failed to open output file");
					results_free(res);
					return;
				}
				stype = 1;
				break;
			} else if(paramstring[i] == '|') {
				stream = popen(paramstring + i + 1, "w");
				if(!stream) {
					perror("Failed to open pipe");
					results_free(res);
					return;
				}
				stype = 2;
				break;
			}
		}

		if(stype == 0) {
			default_pager = getenv("DBSH_DEFAULT_PAGER");
			if(default_pager) {
				stream = popen(default_pager, "w");
				if(!stream) {
					perror("Failed to open pipe");
					results_free(res);
					return;
				}
				stype = 2;
			}
		}

		output_results(res, action, stream);
		results_free(res);

		switch(stype) {
		case 1:
			fclose(stream);
			break;
		case 2:
			pclose(stream);
			break;
		}
	}
}

static void edit(sql_buffer *sqlbuf)
{
	char *editor;
	char path[1024];
	char cmd[2014];
	int f;

	editor = getenv("EDITOR");
	if(!editor) editor = getenv("VISUAL");
	if(!editor) editor = "vi";

	// FIXME: make this safe
	snprintf(path, 1024, "/tmp/dbsh-edit.%d", getpid());

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

static void print(sql_buffer *sqlbuf)
{
	fwrite(sqlbuf->buf, 1, sqlbuf->next, stdout);
	fputc('\n', stdout);
}

void run_action(SQLHDBC *connp, sql_buffer *sqlbuf, char action, char *paramstring)
{
	switch(action) {
	case 'e':  // edit
		edit(sqlbuf);
		print(sqlbuf);
		break;
	case 'l':  // load
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':  // print
		print(sqlbuf);
		break;
	case 's':  // save
		// TODO: save to named buffer
		break;
	default:
		go(connp, sqlbuf, action, paramstring);
		break;
	}
}
