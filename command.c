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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "err.h"
#include "rc.h"
#include "results.h"


extern char **environ;


static results *set(const char *name, const char *value)
{
	results *res = results_alloc();

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

		results_set_rows(res, 1);

		if(!(res->data[0][0] = strdup(name))) err_system();
		if(!(res->data[0][1] = strdup(value))) err_system();

		free(prefixed_name);
	} else {
		char **v, *name_ptr, *value_ptr;
		int i;

		if(!(res->data = calloc(1024, sizeof(char **)))) err_system();

		for(v = environ; *v; v++) {
			if(!strncmp(*v, "DBSH_", 5)) {

				name_ptr = *v + 5;

				value_ptr = strchr(name_ptr, '=');
				if(!value_ptr) continue;

				if(!(res->data[res->nrows] = calloc(2, sizeof(char *)))) err_system();
				if(!(res->data[res->nrows][0] = calloc((value_ptr - name_ptr) + 1, sizeof(char)))) err_system();

				for(i = 0; name_ptr[i] != '='; i++) {
					res->data[res->nrows][0][i] = tolower(name_ptr[i]);
				}

				if(!(res->data[res->nrows][1] = strdup(++value_ptr))) err_system();
				res->nrows++;
			}
		}
	}

	return res;
}

results *run_command(SQLHDBC *connp, char *line)
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
		res = get_tables(*connp, "%", 0, 0);
	} else if(!strcmp(command, "columns")) {
		res = get_columns(*connp, params[0], params[1], params[2]);
	} else if(!strcmp(command, "info")) {
		res = db_conn_details(*connp);
	} else if(!strcmp(command, "reconnect")) {
		if(!db_reconnect(connp, params[0])) res = db_conn_details(*connp);
	} else if(!strcmp(command, "schemas")) {
		res = get_tables(*connp, "%", "%", 0);
	} else if(!strcmp(command, "set")) {
		res = set(params[0], params[1]);
	} else if(!strcmp(command, "tables")) {
		res = get_tables(*connp, params[0], params[1], params[2]);
	} else {
		printf(_("Unrecognised command: %s\n"), command);
	}

	free(dupline);

	return res;
}
