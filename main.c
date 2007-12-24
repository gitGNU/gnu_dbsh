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

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "db.h"
#include "history.h"
#include "output.h"
#include "prompt.h"
#include "rc.h"
#include "sig.h"


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

void *main_loop(void *c)
{
	SQLHDBC *connp = (SQLHDBC *) c;
	sql_buffer *mainbuf, *prevbuf;
	char *line;
	int len, i;
	char action;
	char *paramstring;

	mainbuf = buffer_alloc(256);
	prevbuf = buffer_alloc(256);

	for(;;) {
		line = readline(prompt_render(*connp, mainbuf));
		if(!line) {
			printf("\n");
			break;
		}

		len = strlen(line);
		if(len) {
			action = 0;

			for(i = 0; i < len; i++) {
				if(strchr(getenv("DBSH_ACTION_CHARS"), line[i])) {
					if(++i < len) {
						if(!strchr(getenv("DBSH_ACTION_CHARS"), line[i])) {
							action = line[i];
							paramstring = line + i;
						}
					} else {
						action = 1;  // default;
						paramstring = "";
					}
				}

				if(action) {

					if(action == 'q') return 0;

					if(mainbuf->next) {
						buffer_append(mainbuf, '\0');
					} else {
						if(prevbuf->next) buffer_copy(mainbuf, prevbuf);
						else break;
					}

					run_action(connp, mainbuf, action, paramstring);

					switch(action) {
					case 'c':
					case 'e':
					case 'p':
						// don't add to history or reset buffer (\c has already reset)
						break;
					default:
						history_add(mainbuf, line + i - 1);
						buffer_copy(prevbuf, mainbuf);
						mainbuf->next = 0;
					}

					break;

				} else {
					buffer_append(mainbuf, line[i]);
				}
			}

			if(!action) {
				buffer_append(mainbuf, '\n');
			}
		}

		free(line);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	SQLHDBC conn;
	char *dsn = 0, *user = 0, *pass = 0;
	int opt, i;
	results *res;

	setlocale(LC_ALL, "");
	read_rc_file();

	while((opt = getopt(argc, argv, "l")) != -1) {
		switch(opt) {
		case 'l':
			list_all_dsns();
			return 0;
			break;
		}
	}

	if(argc - optind < 1) {
		usage(argv[0]);
		return 1;
	}

	dsn = argv[optind++];
	if(argc - optind > 0) user = argv[optind++];
	if(argc - optind > 0) pass = argv[optind++];

	conn = db_connect(dsn, user, pass);

	if(pass) for(i = 0; i < strlen(pass); i++) pass[i] = 'x';

	res = db_conn_details(conn);
	output_results(res, 1, stdout);

	history_start();
	signal_handler_install();

	main_loop(&conn);

	history_end();

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);

	return 0;
}

