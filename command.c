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

	res = res_alloc();
	res_set_cols(res, 1, topic);
	res_add_row(res, text);

	return res;
}

static results *get_copying()
{
	results *res = res_alloc();
	res_set_cols(res, 1, _("COPYING"));
	res_add_row(res, GPL_COPYING);
	return res;
}

static results *get_warranty()
{
	results *res = res_alloc();
	res_set_cols(res, 1, _("WARRANTY"));
	res_add_row(res, GPL_WARRANTY);
	return res;
}

static results *set(const char *name, const char *value)
{
	results *res = res_alloc();

	res_set_cols(res, 2, _("name"), _("value"));

	if(name) {
		char *prefixed_name;

		prefixed_name = prefix_var_name(name);

		if(value) {
			if(setenv(prefixed_name, value, 1) == -1) {
				char buf[64];
				strerror_r(errno, buf, 64);
				res_add_warning(res, buf);
			}
		} else {
			value = getenv(prefixed_name);
			if(!value) res_add_warning(res, _("Variable not set"));
		}

		res_add_row(res, name, value);

		free(prefixed_name);
	} else {
		char **v, *name, *value;

		for(v = environ; *v; v++) {
			if(!strncmp(*v, "DBSH_", 5)) {

				if(!(name = strdup(*v + 5))) err_system();

				for(value = name; *value && *value != '='; value++) {
					*value = tolower(*value);
				}

				if(*value) {
					*value++ = 0;
					res_add_row(res, name, value);
				}

				free(name);
			}
		}
	}

	return res;
}

static void unset(const char *name)
{
	char *prefixed_name;

	prefixed_name = prefix_var_name(name);

	if(!strcmp(prefixed_name, "DBSH_ACTION_CHARS") ||
	   !strcmp(prefixed_name, "DBSH_COMMAND_CHARS") ||
	   !strcmp(prefixed_name, "DBSH_DEFAULT_ACTION") ||
	   !strcmp(prefixed_name, "DBSH_PROMPT"))
		printf(_("Cannot unset variable '%s'\n"), name);
	else unsetenv(prefixed_name);

	free(prefixed_name);
}

#define SYNTAX(p) printf(_("Syntax: %s %s\n"), c, p)

results *run_command(buffer *buf)
{
	parsed_line *l;
	results *res = 0;
	char *c, *p1, *p2;

	l = parse_buffer(buf);
	if(l->nchunks < 1) return 0;

	c  = l->chunks[0] + 1;
	p1 = l->nchunks > 1 ? l->chunks[1] : 0;
	p2 = l->nchunks > 2 ? l->chunks[2] : 0;

	// Help commands
	if(*c == 'h') res = get_help(p1 ? p1 : "intro");
	else if(!strncmp(c, "cop", 3)) res = get_copying();
	else if(!strncmp(c, "war", 3)) res = get_warranty();

	// Catalog commands
	else if(!strncmp(c, "cat", 3)) {
		res = get_tables(SQL_ALL_CATALOGS, "", "");
	} else if(!strncmp(c, "sch", 3)) {
		res = get_tables("", SQL_ALL_SCHEMAS, "");
	} else if(!strncmp(c, "tab", 3)) {
		res = db_list_tables(p1);
	} else if(!strncmp(c, "col", 3)) {
		if(p1) res = db_list_columns(p1);
		else SYNTAX(_("<table>"));
	}

	// Other commands
	else if(!strcmp(c, "set")) {
		res = set(p1, p2);
	} else if(!strcmp(c, "unset")) {
		if(p1) unset(p1);
		else SYNTAX(_("<variable>"));
	} else if(!strncmp(c, "inf", 3)) {
		res = db_conn_details();
	}

	else printf(_("Unrecognised command: %s\n"), c);

	free_parsed_line(l);

	return res;
}
