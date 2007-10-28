#ifndef DB_H
#define DB_H

#include <sql.h>
#include <sqlext.h>

#include "results.h"


SQLHENV alloc_env();
void list_all_dsns(SQLHENV);
SQLHDBC connect_dsn(SQLHENV, const char *, const char *, const char *);
void get_catalog_name(SQLHDBC, char *, int);
db_results *execute_query(SQLHDBC, const char *);
void cancel_query();
db_results *get_tables(SQLHDBC, const char *, const char *, const char *);
db_results *get_columns(SQLHDBC, const char *, const char *, const char *);
void free_results(db_results *);

#endif
