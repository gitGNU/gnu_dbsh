#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sql.h>

#include "common.h"
#include "buffer.h"
#include "db.h"


static int get_lnum(sql_buffer *buf)
{
	int i, n = 1;

	for(i = 0; i < buf->next; i++) if(buf->buf[i] == '\n') n++;

	return n;
}

const char *prompt_render(SQLHDBC conn, sql_buffer *buf)
{
	static char prompt[256];

	const char *tpl;
	char *p;
	int i, l;
	char info[64];

	tpl = getenv("DBSH_PROMPT");
	p = prompt;

	// TODO: check for overflow

	l = strlen(tpl);
	for(i = 0; i < l; i++) {
		switch(tpl[i]) {
		case 'd':
			db_info(conn, SQL_DATA_SOURCE_NAME, info, 64);
			strcpy(p, info);
			p += strlen(info);
			break;
		case 'l':
			p += sprintf(p, "%d", get_lnum(buf));
			break;
		default:
			*p++ = tpl[i];
		}
	}

	return prompt;
}

