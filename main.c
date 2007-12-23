/*
 * TODO: trap Ctrl-C and kill query instead (SQLCancel?), or just clear buffer if we're not running a query
 *       multiple result sets (SQLMoreResults)
 *       batch mode?
 *       tab completion - commands as a minimum, maybe tables etc too?
 *       regression tests using SQLite
 *       cope with result set being too large to fit in memory
 *       separate history by DSN?
 *       newlines in data break horizontal display
 *       reconnect if connection dies
 *       do away with line numbers, since they don't make sense anyway? (or make them make sense by counting newlines?)
 *       specify editor as argument to \e? (or named buffer?)
 *       text truncated?
 *       actions as shortcuts for commands, eg \r for reconnect (or do away with commands and only have actions?)
 *
 *       prepared statement support:
 *       SELECT * FROM <table> WHERE id = ?
 *       \g 1
 *       \g 2
 *       etc
 *
 *       commands:
 *       *autocommit on|off
 */

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <libguile.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "db.h"
#include "history.h"
#include "output.h"
#include "prompt.h"
#include "rc.h"
#include "signal.h"


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

void *main_loop(void *c)
{
	SQLHDBC *connp = (SQLHDBC *) c;
	sql_buffer *mainbuf;
	char *line;
	int len, i;
	char action;
	char *paramstring;

	mainbuf = buffer_alloc(1024);

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
					buffer_append(mainbuf, '\0');

					if(action == 'q') return 0;
					if(action != 'c') {
						run_action(connp, mainbuf, action, paramstring);
						history_add(mainbuf, line + i - 1);
					}

					break;
				} else {
					buffer_append(mainbuf, line[i]);
				}
			}

			if(action) mainbuf->next = 0;
			else buffer_append(mainbuf, '\n');
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

	scm_with_guile(main_loop, &conn);

	history_end();

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);

	return 0;
}

