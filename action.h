#ifndef ACTION_H
#define ACTION_H

#include "buffer.h"
#include "db.h"

int run_action(SQLHDBC, sql_buffer *, char, char *);

#endif
