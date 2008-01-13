#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>
#include <wchar.h>

struct stream {
	FILE *f;
	mbstate_t ps;
};

stream *stream_create(FILE *);
void stream_reset(stream *);
void stream_puts(stream *, const char *);
void stream_write(stream *, const char *, size_t);
void stream_printf(stream *, const char *, ...) __attribute__ ((format(printf, 2, 3)));
void stream_putwc(stream *, wchar_t);
void stream_putws(stream *, const wchar_t *);
void stream_space(stream *);
void stream_newline(stream *);

#endif
