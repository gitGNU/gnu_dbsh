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

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "db.h"
#include "prompt.h"
#include "rc.h"
#include "rl.h"
#include "sig.h"


SQLHDBC conn;
buffer *mainbuf, *prevbuf;


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

int process_line(char *line)
{
	char *actionchars, action, *paramstring;
	buffer *tempbuf;

	action = 0;
	actionchars = getenv("DBSH_ACTION_CHARS");

	for(; *line; line++) {

		if(strchr(actionchars, *line)) {
			if(*++line) {
				if(!strchr(actionchars, *line)) {
					action = *line;
					paramstring = line;
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
					rl_history_add(mainbuf, "");
				} else {
					rl_history_add(mainbuf, line - 1);
				}

				tempbuf = prevbuf;
				prevbuf = mainbuf;
				mainbuf = tempbuf;
			}

			mainbuf->next = 0;
			return 0;

		} else {
			buffer_append(mainbuf, *line);
		}
	}

	if(mainbuf->next) buffer_append(mainbuf, '\n');
	return 0;
}

int main(int argc, char *argv[])
{
	char *dsn = 0, *user = 0, *pass = 0;
	int opt;
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
			puts(PACKAGE_STRING);
			return 0;
			break;
		}
	}

	if(argc - optind < 1) {
		usage(argv[0]);
		return 1;
	}

	puts(PACKAGE_STRING " Copyright (C) 2007, 2008 Ben Spencer\n"
	     "This program comes with ABSOLUTELY NO WARRANTY; "
	     "for details type `/warranty; | more'\n"
	     "This is free software: "
	     "you are welcome to modify and redistribute it\n"
	     "under certain conditions; for details type "
	     "`/copying; | more'\n"
	     "Type `/help;' for help or `\\q' to quit.\n");

	dsn = argv[optind++];
	if(argc - optind > 0) user = argv[optind++];
	if(argc - optind > 0) pass = argv[optind++];

	conn = db_connect(dsn, user, pass);

	if(pass) for(; *pass; pass++) *pass = 'x';
	rl_history_start();
	signal_handler_install();

	mainbuf = buffer_alloc(256);
	prevbuf = buffer_alloc(256);

	/* Main loop */
	for(;;) {
		line = rl_readline(prompt_render(conn, mainbuf));
		if(!line || process_line(line)) {
			fputc('\n', stdout);
			break;
		}
		free(line);
	}

	rl_history_end();

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);

	return 0;
}

