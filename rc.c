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

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "err.h"
#include "rc.h"


const char *get_rc_dir()
{
	static char *rc_dir = 0;

	char *home;
	size_t len;
	struct stat st;

	if(!rc_dir) {

		if((home = getenv("HOME"))) {
			len = strlen(home) + strlen(PACKAGE) + 3;
			if(!(rc_dir = malloc(len))) err_system();
			snprintf(rc_dir, len, "%s/.%s", home, PACKAGE);

			if(stat(rc_dir, &st)) {
				if(errno == ENOENT) {
					if(mkdir(rc_dir, 0755)) {
						perror("Failed to create rc dir");
						exit(1);
					}
				} else err_system();
			} else {
				if(!S_ISDIR(st.st_mode))
					err_fatal("%s is not a directory", rc_dir);
			}
		}
	}

	return rc_dir;
}

void read_rc_file()
{
	char buffer[1024];
	const char *rc_dir;
	char *rc_file, *name, *value;
	int len;
	FILE *rc;

	if((rc_dir = get_rc_dir())) {

		len = strlen(rc_dir) + strlen(PACKAGE) + 4;
		if(!(rc_file = malloc(len))) err_system();

		snprintf(rc_file, len, "%s/%src", rc_dir, PACKAGE);

		if((rc = fopen(rc_file, "r"))) {

			while(fgets(buffer, 1024, rc)) {

				name = strtok(buffer, "=\n");
				value = strtok(NULL, "=\n");

				if(name && value) {
					name = prefix_var_name(name);
					if(setenv(name, value, 0)) err_system();
					free(name);
				}
			}

			fclose(rc);
		}

		free(rc_file);
	}

	// Default settings
	if(!getenv("DBSH_ACTION_CHARS"))   setenv("DBSH_ACTION_CHARS",   "\\;", 1);
	if(!getenv("DBSH_COMMAND_CHARS"))  setenv("DBSH_COMMAND_CHARS",  "/", 1);
	if(!getenv("DBSH_DEFAULT_ACTION")) setenv("DBSH_DEFAULT_ACTION", "g", 1);
	if(!getenv("DBSH_PROMPT"))         setenv("DBSH_PROMPT",         "d l> ", 1);
}

char *prefix_var_name(const char *name)
{
	char *prefixed_name;
	int i, j, l;

	l = strlen(name);

	if(!(prefixed_name = malloc(l + 6))) err_system();

	strcpy(prefixed_name, "DBSH_");

	j = 5;
	for(i = 0; i < l; i++) {
		if(!isspace(name[i]))
			prefixed_name[j++] = toupper(name[i]);
	}

	prefixed_name[j] = 0;

	return prefixed_name;
}
