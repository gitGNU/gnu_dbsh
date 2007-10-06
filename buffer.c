#include <stdio.h>

#include "buffer.h"

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
