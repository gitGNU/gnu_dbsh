#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "rc.h"


extern char **environ;


static db_results *set(const char *name, const char *value)
{
	db_results *res = calloc(1, sizeof(struct db_results));

	if(!res) {
		perror("Error allocating result set");
		return 0;
	}

	res->ncols = 2;
	res->cols = calloc(2, sizeof(char *));
	res->cols[0] = strdup(_("name"));
	res->cols[1] = strdup(_("value"));


	if(name) {
		char *prefixed_name;

		prefixed_name = prefix_var_name(name);
		if(!prefixed_name) goto set_error;

		if(value) {
			if(setenv(prefixed_name, value, 1) == -1) {
				res->nwarnings = 1;
				res->warnings = calloc(1, sizeof(char *));
				res->warnings[0] = malloc(64);
				strerror_r(errno, res->warnings[0], 64);
			}
		} else {
			value = getenv(prefixed_name);
			if(!value) {
				value = "";
				res->nwarnings = 1;
				res->warnings = calloc(1, sizeof(char *));
				res->warnings[0] = strdup(_("Variable not set"));
			}
		}

		res->nrows = 1;
		res->data = calloc(1, sizeof(char **));
		res->data[0] = calloc(2, sizeof(char *));
		res->data[0][0] = strdup(name);
		res->data[0][1] = strdup(value);

		free(prefixed_name);
	} else {
		char **v, *name_ptr, *value_ptr;
		int i;

		res->data = calloc(1024, sizeof(char **));

		for(v = environ; *v; v++) {
			if(!strncmp(*v, "DBSH_", 5)) {

				name_ptr = *v + 5;

				value_ptr = strchr(name_ptr, '=');
				if(!value_ptr) continue;

				res->data[res->nrows] = calloc(2, sizeof(char *));
				res->data[res->nrows][0] = calloc((value_ptr - name_ptr) + 1, sizeof(char));

				for(i = 0; name_ptr[i] != '='; i++) {
					res->data[res->nrows][0][i] = tolower(name_ptr[i]);
				}

				res->data[res->nrows][1] = strdup(++value_ptr);
				res->nrows++;
			}
		}
	}

	return res;

	set_error:
	perror("Error");
	free_results(res);
	return 0;
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
		if(params[i] && !strcmp(params[i], "NULL")) params[i] = 0;
		line = 0;
	}

	if(!strcmp(command, "catalogs")) {
		res = get_tables(conn, "%", 0, 0);
	} else if(!strcmp(command, "columns")) {
		res = get_columns(conn, params[0], params[1], params[2]);
	} else if(!strcmp(command, "schemas")) {
		res = get_tables(conn, "%", "%", 0);
	} else if(!strcmp(command, "set")) {
		res = set(params[0], params[1]);
	} else if(!strcmp(command, "tables")) {
		res = get_tables(conn, params[0], params[1], params[2]);
	} else {
		printf(_("Unrecognised command: %s\n"), command);

	}

	return res;
}
