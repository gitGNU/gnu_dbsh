#ifndef ERR_H
#define ERR_H

void err_fatal(const char *, ...)  __attribute__ ((format(printf, 1, 2)));
void err_system();

#endif
