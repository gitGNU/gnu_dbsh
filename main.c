/*
 * TODO: trap Ctrl-C and kill query instead (SQLCancel?)
 *       multiple result sets (SQLMoreResults)
 *       pay more attention to return values (eg SQL_SUCCESS_WITH_INFO) and display info/warnings
 *
 *       prepared statement support:
 *       SELECT * FROM <table> WHERE id = ?
 *       \g 1
 *       \g 2
 *       etc
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "db.h"
#include "output.h"


typedef struct {
	char *buf;
	size_t len;
	int next;
} sql_buffer;


void usage(const char *cmd)
{
	printf(_("Usage: %s -l\n       %s <dsn> [<username>] [<password>]\n"),
	       cmd, cmd);
}

int add_to_buffer(sql_buffer *buf, char c)
{
	if(buf->next >= buf->len) {
		buf->next = 0;
		printf("SQL Buffer size exceeded - contents discarded\n");
		return 0;
	}

	buf->buf[buf->next++] = c;

	return 1;
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


db_results *run_command(SQLHDBC conn, char *line)
{
	int i;
	char command[32] = "";
	char *saveptr;
	char *params[4];
	db_results *res = 0;

	for(i = 0; i < 31 && line[i+1] && line[i+1] != ' '; i++) {
		command[i] = tolower(line[i+1]);
	}
	command[i] = 0;

	// TODO: parse properly, allow quoting / escaping etc
	line += i + 1;
	for(i = 0; i < 4; i++) {
		params[i] = strtok_r(line, " ", &saveptr);
		line = 0;
	}

	if(!strcmp(command, "columns")) {
		res = get_columns(conn, params[0], params[1], params[2]);
	} else if(!strcmp(command, "tables")) {
		res = get_tables(conn, params[0], params[1], params[2]);
	} else {
		printf(_("Unrecognised command: %s\n"), command);

	}

	return res;
}

int run_action(SQLHDBC conn, sql_buffer *sqlbuf, char action)
{
	int reset = 0;
	db_results *res;

	switch(action) {
	case 'c':  // CSV
	case 'g':  // horizontal
	case 'G':  // vertical
	case 'h':  // HTML
	case 'j':  // JSON
	case 't':  // TSV
	case 'x':  // XML
		if(sqlbuf->buf[0] == '*') {  // TODO: configurable command character
			res = run_command(conn, sqlbuf->buf);
		} else {
			res = execute_query(conn, sqlbuf->buf);
		}
		if(res) {
			output_results(res, action, stdout);
			free_results(res);
		}
		reset = 1;
		break;
	case 'e':
		// TODO: edit
		break;
	case 'l':
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':
		printf("%s\n", sqlbuf->buf);
		sqlbuf->next--;
		break;
	case 's':
		// TODO: save to named buffer
		break;
	}

	return reset;
}

void main_loop(const char *dsn, SQLHDBC conn)
{
	char prompt[16];
	char *line;
	int lnum, len, i;
	sql_buffer sqlbuf;
	int reset = 0;

	sqlbuf.buf  = malloc(1024);
	sqlbuf.len  = 1024;
	sqlbuf.next = 0;

	lnum = 1;

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

					if(!add_to_buffer(&sqlbuf, '\0')) {
						break;
					}

					delete_latest_history(lnum);
					add_history(sqlbuf.buf);
					write_history(get_history_filename());

					if(i < (len - 1)) action = line[++i];
					else action = 'g';

					if(action == 'q') return;
					reset = run_action(conn, &sqlbuf, action);

				} else {
					if(reset) {
						sqlbuf.next = 0;
						reset = 0;
					}

					if(!add_to_buffer(&sqlbuf, line[i])) {
						reset = 1;
						break;
					}
				}
			}

			if(!reset) {
				if(add_to_buffer(&sqlbuf, ' ')) lnum++;
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
