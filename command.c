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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "gplv3.h"
#include "help.h"
#include "err.h"
#include "parser.h"
#include "rc.h"
#include "results.h"

extern char **environ;


static results *get_help(const char *topic)
{
	const char *text;
	results *res;

	if(!strcmp(topic, "intro")) text = HELP_INTRO;
	else text = HELP_NOTFOUND;

	res = results_single_alloc();
	results_set_cols(res, 1, topic);
	results_add_row(res, text);

	return res;
}

static results *get_copying()
{
	results *res = results_single_alloc();
	results_set_cols(res, 1, _("COPYING"));
	results_add_row(res, GPL_COPYING);
	return res;
}

static results *get_warranty()
{
	results *res = results_single_alloc();
	results_set_cols(res, 1, _("WARRANTY"));
	results_add_row(res, GPL_WARRANTY);
	return res;
}

static results *set(const char *name, const char *value)
{
	results *res = results_single_alloc();

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
			if(!value) results_set_warnings(res, 1, _("Variable not set"));
		}

		results_add_row(res, name, value);

		free(prefixed_name);
	} else {
		char **v, *name_ptr, *value_ptr;
		row **rp;
		int i;

		rp = &(res->sets->rows);

		for(v = environ; *v; v++) {
			if(!strncmp(*v, "DBSH_", 5)) {
				name_ptr = *v + 5;
				value_ptr = strchr(name_ptr, '=');
				if(!value_ptr) continue;

				*rp = results_row_alloc(2);

				if(!((*rp)->data[0] = calloc((value_ptr - name_ptr) + 1, sizeof(char)))) err_system();

				for(i = 0; name_ptr[i] != '='; i++)
					(*rp)->data[0][i] = tolower(name_ptr[i]);

				if(!((*rp)->data[1] = strdup(++value_ptr))) err_system();

				res->sets->nrows++;
				rp = &(*rp)->next;
			}
		}
	}

	return res;
}

results *run_command(SQLHDBC conn, buffer *buf)
{
	parsed_line *l;
	results *res = 0;

	l = parse_buffer(buf);
	if(l->nchunks < 1) return 0;

	// Help commands
	if(l->chunks[0][1] == 'h') res = get_help(l->nchunks > 1 ? l->chunks[1] : "intro");
	else if(!strncmp(l->chunks[0] + 1, "cop", 3)) res = get_copying();
	else if(!strncmp(l->chunks[0] + 1, "war", 3)) res = get_warranty();

	// Catalog commands
	else if(!strncmp(l->chunks[0] + 1, "cat", 3)) {
		res = get_tables(conn, "%", 0, 0);
	} else if(!strncmp(l->chunks[0] + 1, "sch", 3)) {
		res = get_tables(conn, "%", "%", 0);
	} else if(!strncmp(l->chunks[0] + 1, "tab", 3)) {
		res = get_tables(conn,
				 l->nchunks > 1 ? l->chunks[1] : 0,
				 l->nchunks > 2 ? l->chunks[2] : 0,
				 l->nchunks > 3 ? l->chunks[3] : 0);
	} else if(!strncmp(l->chunks[0] + 1, "col", 3)) {
		res = get_columns(conn,
				 l->nchunks > 1 ? l->chunks[1] : 0,
				 l->nchunks > 2 ? l->chunks[2] : 0,
				 l->nchunks > 3 ? l->chunks[3] : 0);
	}

	// Other commands
	else if(!strcmp(l->chunks[0] + 1, "set")) {
		res = set(l->nchunks > 1 ? l->chunks[1] : 0,
			  l->nchunks > 2 ? l->chunks[2] : 0);
	} else if(!strncmp(l->chunks[0] + 1, "inf", 3)) {
		res = db_conn_details(conn);
	}

	else printf(_("Unrecognised command: %s\n"), l->chunks[0] + 1);

	free_parsed_line(l);

	return res;
}
