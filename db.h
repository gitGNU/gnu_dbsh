#ifndef DB_H
#define DB_H

#include <sys/time.h>
#include <time.h>

#include <sql.h>
#include <sqlext.h>

SQLHENV alloc_env();
void list_all_dsns();
SQLHDBC connect_dsn(SQLHENV, const char *, const char *, const char *);
results *db_conn_info(SQLHDBC);
results *execute_query(SQLHDBC, const char *);
void cancel_query();
results *get_tables(SQLHDBC, const char *, const char *, const char *);
results *get_columns(SQLHDBC, const char *, const char *, const char *);

#endif
