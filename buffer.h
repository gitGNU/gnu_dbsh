#ifndef BUFFER_H
#define BUFFER_H

#include <sys/types.h>

typedef struct {
	char *buf;
	size_t len;
	int next;
} sql_buffer;

sql_buffer *buffer_alloc(size_t);
int buffer_append(sql_buffer *, char);
void buffer_free(sql_buffer *);

#endif
