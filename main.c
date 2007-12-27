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
#include "prompt.h"
#include "rc.h"
#include "sig.h"


SQLHDBC conn;
sql_buffer *mainbuf, *prevbuf;


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

int process_line(char *line)
{
	int len, i;
	char *actionchars, action, *paramstring;
	sql_buffer *tempbuf;

	if(!(len = strlen(line))) return 0;
	action = 0;
	actionchars = getenv("DBSH_ACTION_CHARS");

	for(i = 0; i < len; i++) {

		if(strchr(actionchars, line[i])) {
			if(++i < len) {
				if(!strchr(actionchars, line[i])) {
					action = line[i];
					paramstring = line + i;
				}
			} else {
				action = 1;  // default;
				paramstring = "";
			}
		}

		if(action) {
			if(action == 'q') return -1;

			if(!mainbuf->next) {
				if(prevbuf->next) {
					tempbuf = mainbuf;
					mainbuf = prevbuf;
					prevbuf = tempbuf;
				} else return 0;
			}

			if(action != 'c') {
				run_action(&conn, mainbuf,
					   action, paramstring);

				if(action == 'e' || action == 'p') {
					history_add(mainbuf, "");
				} else {
					history_add(mainbuf, line + i - 1);
				}

				tempbuf = prevbuf;
				prevbuf = mainbuf;
				mainbuf = tempbuf;
			}

			mainbuf->next = 0;
			return 0;

		} else {
			buffer_append(mainbuf, line[i]);
		}
	}

	buffer_append(mainbuf, '\n');
	return 0;
}

int main(int argc, char *argv[])
{
	char *dsn = 0, *user = 0, *pass = 0;
	int opt, i;
	char *line;

	setlocale(LC_ALL, "");
	read_rc_file();

	while((opt = getopt(argc, argv, "lv")) != -1) {
		switch(opt) {
		case 'l':
			list_all_dsns();
			return 0;
			break;
		case 'v':
			printf("%s\n", PACKAGE_STRING);
			return 0;
			break;
		}
	}

	if(argc - optind < 1) {
		usage(argv[0]);
		return 1;
	}

	puts(PACKAGE_STRING " Copyright (C) 2007 Ben Spencer\n"
	     "This program comes with ABSOLUTELY NO WARRANTY; "
	     "for details type:\n> *warranty\\T | more\n"
	     "This is free software: "
	     "you are welcome to modify and redistribute it\n"
	     "under certain conditions; for details type:\n"
	     "> *copying\\T | more\n\n");

	dsn = argv[optind++];
	if(argc - optind > 0) user = argv[optind++];
	if(argc - optind > 0) pass = argv[optind++];

	conn = db_connect(dsn, user, pass);

	if(pass) for(i = 0; i < strlen(pass); i++) pass[i] = 'x';
	history_start();
	signal_handler_install();

	mainbuf = buffer_alloc(256);
	prevbuf = buffer_alloc(256);

	/* Main loop */
	for(;;) {
		line = readline(prompt_render(conn, mainbuf));
		if(!line || process_line(line)) {
			printf("\n");
			break;
		}
		free(line);
	}

	history_end();

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);

	return 0;
}

