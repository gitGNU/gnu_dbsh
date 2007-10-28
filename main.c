/*
 * TODO: trap Ctrl-C and kill query instead (SQLCancel?), or just clear buffer if we're not running a query
 *       multiple result sets (SQLMoreResults)
 *       batch mode?
 *       tab completion - commands as a minimum, maybe tables etc too?
 *       regression tests using SQLite
 *       pipe to less -F by default if no pipe or redirect specified? (or user-configurable default pipe)
 *       cope with result set being too large to fit in memory
 *
 *       prepared statement support:
 *       SELECT * FROM <table> WHERE id = ?
 *       \g 1
 *       \g 2
 *       etc
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <libguile.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "db.h"
#include "rc.h"
#include "signal.h"


struct main_data
{
	const char *dsn;
	SQLHENV env;
	SQLHDBC conn;
};

void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

void *main_loop(void *d)
{
	struct main_data *data = (struct main_data *) d;
	sql_buffer *mainbuf;
	char prompt[16];
	char *line;
	int lnum, len, i;
	int reset;

	mainbuf = buffer_alloc(1024);  // TODO: make buffer size configurable
	if(!mainbuf) {
		printf(_("Failed to allocate SQL buffer\n"));
		exit(1);
	}

	lnum = 1;
	reset = 0;

	for(;;) {
		snprintf(prompt, 16, "%s %d> ", data->dsn, lnum);  // TODO: configurable prompt (eg current catalog, fetched using SQLGetInfo)

		line = readline(prompt);
		if(!line) {
			printf("\n");
			break;
		}

		len = strlen(line);
		if(len) {

			// TODO: interpret single character as action?

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
					run_action(data->conn, mainbuf, action, paramstring);

					if(mainbuf->next > 1) {
						char *histentry;
						char *p;

						histentry = p = malloc(mainbuf->next + strlen(line + i) + 1);
						if(histentry) {
							strcpy(histentry, mainbuf->buf);
							strcat(histentry, line + i);
							while((p = strchr(p, '\n'))) *p = ' ';

							add_history(histentry);
							free(histentry);

							// This can be removed once this client is stable enough to trust
							write_history(get_history_filename());
						}
					}

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
	int opt, i;
	struct main_data data;
	char *user = (char *) 0, *pass = (char *) 0;


	data.env = alloc_env();

	while((opt = getopt(argc, argv, "l")) != -1) {
		switch(opt) {
		case 'l':
			list_all_dsns(data.env);
			return 0;
			break;
		}
	}

	if(argc - optind < 1) {
		usage(argv[0]);
		return 1;
	}

	data.dsn = argv[optind++];
	if(argc - optind > 0) user = argv[optind++];
	if(argc - optind > 0) pass = argv[optind++];

	data.conn = connect_dsn(data.env, data.dsn, user, pass);

	if(pass) for(i = 0; i < strlen(pass); i++) pass[i] = 'x';

	read_rc_file();

	using_history();
	read_history(get_history_filename());

	signal_handler_install();

	scm_with_guile(main_loop, &data);

	write_history(get_history_filename());

	SQLDisconnect(data.conn);
	SQLFreeHandle(SQL_HANDLE_DBC, data.conn);
	SQLFreeHandle(SQL_HANDLE_ENV, data.env);

	return 0;
}

