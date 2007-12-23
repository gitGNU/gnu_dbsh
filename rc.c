#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "err.h"
#include "rc.h"


void read_rc_file()
{
	char buffer[1024];
	char *home, *name, *value;
	FILE *rc;

	home = getenv("HOME");
	if(!home) return;

	snprintf(buffer, 1024, "%s/.dbshrc", home);

	if(!(rc = fopen(buffer, "r"))) return;

	while(fgets(buffer, 1024, rc)) {

		name = strtok(buffer, "=\n");
		value = strtok(NULL, "=\n");

		if(name && value) {
			name = prefix_var_name(name);
			setenv(name, value, 1);
			free(name);
		}
	}

	fclose(rc);


	// Default settings

	if(!getenv("DBSH_ACTION_CHARS"))   setenv("DBSH_ACTION_CHARS",   "\\", 1);
	if(!getenv("DBSH_COMMAND_CHARS"))  setenv("DBSH_COMMAND_CHARS",  "*", 1);
	if(!getenv("DBSH_DEFAULT_ACTION")) setenv("DBSH_DEFAULT_ACTION", "g", 1);
	if(!getenv("DBSH_PROMPT"))         setenv("DBSH_PROMPT",         "{dsn} {line}> ", 1);
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
