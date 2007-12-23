#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void err_fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}

void err_system(const char *s)
{
	perror("");

	exit(1);
}

