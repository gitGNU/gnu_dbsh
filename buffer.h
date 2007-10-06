#ifndef BUFFER_H
#define BUFFER_H

#include <sys/types.h>

typedef struct {
	char *buf;
	size_t len;
	int next;
} sql_buffer;

int add_to_buffer(sql_buffer *, char);

#endif
