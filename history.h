#ifndef HISTORY_H
#define HISTORY_H

void history_start();
void history_add(sql_buffer *, const char *);
void history_end();

#endif
