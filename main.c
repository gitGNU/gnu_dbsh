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
#include "rc.h"
#include "signal.h"


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

void *main_loop(void *c)
{
	SQLHDBC conn = (SQLHDBC) c;
	sql_buffer *mainbuf;
	char dsn[64];
	char prompt[64];  // TODO: dynamic?
	char *line;
	int lnum, len, i;
	int reset;

	mainbuf = buffer_alloc(1024);  // TODO: make buffer size configurable

	lnum = 1;
	reset = 0;

	db_info(conn, SQL_DATA_SOURCE_NAME, dsn, 64);

	for(;;) {
		snprintf(prompt, 64, "%s %d> ", dsn, lnum);  // TODO: configurable prompt (eg current catalog, fetched using SQLGetInfo)

		line = readline(prompt);
		if(!line) {
			printf("\n");
			break;
		}

		len = strlen(line);
		if(len) {

			for(i = 0; i < len; i++) {

				// TODO: configurable escape char(s)

				if(line[i] == '\\' || line[i] == ';') {  // TODO: allow escaping with double backslash

					char action;
					char *paramstring;

					if(!buffer_append(mainbuf, '\0')) {
						break;
					}

					if(i < (len - 1)) {
						action = line[i + 1];
						paramstring = line + i + 1;
					} else {
						action = 'g';
						paramstring = "";
					}

					if(action == 'q') return 0;
					run_action(conn, mainbuf, action, paramstring);

					history_add(mainbuf, line + i);

					reset = 1;

					break;

				} else {
					if(reset) {
						mainbuf->next = 0;
						reset = 0;
					}

					if(!buffer_append(mainbuf, line[i])) {
						reset = 1;
						break;
					}
				}
			}

			if(!reset) {
				if(buffer_append(mainbuf, '\n')) lnum++;
				else reset = 1;
			}

			if(reset) lnum = 1;
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

	setlocale(LC_ALL, "");


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

	read_rc_file();
	history_start();
	signal_handler_install();

	scm_with_guile(main_loop, conn);

	history_end();

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);

	return 0;
}

