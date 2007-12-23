#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "err.h"

sql_buffer *buffer_alloc(size_t len)
{
	sql_buffer *b;

	if(!(b = malloc(sizeof(sql_buffer)))) err_system();
	if(!(b->buf = malloc(len))) err_system();

	b->len  = len;
	b->next = 0;

	return b;
}

int buffer_append(sql_buffer *buf, char c)
{
	if(buf->next >= buf->len) {
		buf->next = 0;
		printf("SQL Buffer size exceeded - contents discarded\n");
		return 0;
	}

	buf->buf[buf->next++] = c;

	return 1;
}

void buffer_free(sql_buffer *b)
{
	free(b->buf);
	free(b);
}
