#ifndef COMMAND_H
#define COMMAND_H

#include "db.h"

db_results *run_command(SQLHDBC, const char *);

#endif
