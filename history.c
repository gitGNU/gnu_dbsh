#include <stdlib.h>
#include <string.h>
#include <readline/history.h>

#include "buffer.h"


static const char *get_history_filename()
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

void history_start()
{
	using_history();
	read_history(get_history_filename());
}

void history_add(sql_buffer *buf, const char *action_line)
{
	char *histentry;
	char *p;


	histentry = p = malloc(buf->next + strlen(action_line) + 1);
	if(histentry) {
		strcpy(histentry, buf->buf);
		strcat(histentry, action_line);
		while((p = strchr(p, '\n'))) *p = ' ';

		add_history(histentry);
		free(histentry);

		// This can be removed once this client is stable enough to trust
		write_history(get_history_filename());
	}
}

void history_end()
{
	write_history(get_history_filename());
}
