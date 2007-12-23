#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "rc.h"
#include "results.h"


extern char **environ;


static results *set(const char *name, const char *value)
{
	results *res = calloc(1, sizeof(struct results));

	if(!res) {
		perror("Error allocating result set");
		return 0;
	}

	results_set_cols(res, 2, _("name"), _("value"));

	if(name) {
		char *prefixed_name;

		prefixed_name = prefix_var_name(name);

		if(value) {
			if(setenv(prefixed_name, value, 1) == -1) {
				char buf[64];
				strerror_r(errno, buf, 64);
				results_set_warnings(res, 1, buf);
			}
		} else {
			value = getenv(prefixed_name);
			if(!value) {
				value = "";
				results_set_warnings(res, 1, _("Variable not set"));
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
}

results *run_command(SQLHDBC conn, char *line)
{
	int i;
	char command[32] = "";
	char *dupline;
	char *saveptr;
	char *params[4];
	results *res = 0;

	for(i = 0; i < 31 && line[i+1] && !isspace(line[i+1]); i++) {
		command[i] = tolower(line[i+1]);
	}
	command[i] = 0;

	// TODO: parse properly, allow quoting / escaping etc

	dupline = strdup(line);

	dupline += i + 1;
	for(i = 0; i < 4; i++) {
		params[i] = strtok_r(dupline, " \n\t", &saveptr);
		if(params[i] && !strcmp(params[i], "NULL")) params[i] = 0;
		dupline = 0;
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

	free(dupline);

	return res;
}
