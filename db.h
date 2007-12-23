#ifndef DB_H
#define DB_H

#include <sys/time.h>
#include <time.h>

#include <sql.h>
#include <sqlext.h>

void list_all_dsns();
SQLHDBC db_connect(const char *, const char *, const char *);
void db_info(SQLHDBC, SQLUSMALLINT, char *, int);
results *db_conn_details(SQLHDBC);
results *execute_query(SQLHDBC, const char *);
void cancel_query();
results *get_tables(SQLHDBC, const char *, const char *, const char *);
results *get_columns(SQLHDBC, const char *, const char *, const char *);

#endif
