/*
 * TODO: trap Ctrl-C and kill query instead (SQLCancel?), or just clear buffer if we're not running a query
 *       multiple result sets (SQLMoreResults)
 *       pipes and redirects
 *       batch mode?
 *
 *       prepared statement support:
 *       SELECT * FROM <table> WHERE id = ?
 *       \g 1
 *       \g 2
 *       etc
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "db.h"


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

const char *get_history_filename()
{
	static char filename[1024];
	char *home;

	home = getenv("HOME");

	if(home) {
		snprintf(filename, 1024, "%s/.dbsh_history", home);
		return filename;
	}

	return 0;
}

void delete_latest_history(int n)
{
	int p;

	for(p = history_length - 1; n && p >= 0; n--, p--)
		free_history_entry(remove_history(p));
}

void main_loop(const char *dsn, SQLHDBC conn)
{
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
		snprintf(prompt, 16, "%s %d> ", dsn, lnum);  // TODO: configurable prompt (eg current catalog, fetched using SQLGetInfo)

		line = readline(prompt);
		if(!line) {
			printf("\n");
			break;
		}

		len = strlen(line);
		if(len) {
			add_history(line);

			// TODO: interpret single character as action?

			for(i = 0; i < len; i++) {

				// TODO: configurable escape char(s)

				if(line[i] == '\\' || line[i] == ';') {  // TODO: allow escaping with double backslash

					char action;

					if(!buffer_append(mainbuf, '\0')) {
						break;
					}

					if(i < (len - 1)) action = line[++i];
					else action = 'g';

					if(action == 'q') return;
					reset = run_action(conn, mainbuf, action);

					if(reset) {
						delete_latest_history(lnum);
						add_history(mainbuf->buf);
						write_history(get_history_filename());
					}

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
}

int main(int argc, char *argv[])
{
	int opt;
	char *dsn = (char *) 0, *user = (char *) 0, *pass = (char *) 0;
	SQLHENV env;
	SQLHDBC conn;

	env = alloc_env();

	while((opt = getopt(argc, argv, "l")) != -1) {
		switch(opt) {
		case 'l':
			list_all_dsns(env);
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

	conn = connect_dsn(env, dsn, user, pass);

	using_history();
	read_history(get_history_filename());

	main_loop(dsn, conn);

	write_history(get_history_filename());

	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);
	SQLFreeHandle(SQL_HANDLE_ENV, env);

	return 0;
}
