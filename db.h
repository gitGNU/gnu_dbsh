/*
    dbsh - text-based ODBC client
    Copyright (C) 2007, 2008 Ben Spencer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DB_H
#define DB_H

#include <sys/time.h>

#include <sql.h>
#include <sqlext.h>

results *db_drivers_and_dsns();
int db_connect();
void db_reconnect();
void db_close();
SQLSMALLINT db_info(SQLUSMALLINT, char *, int);
SQLINTEGER db_conn_attr(SQLINTEGER, char *, int);
results *db_conn_details();
int db_supports_catalogs();
results *execute_query(const char *, int, parsed_line *);
void db_cancel_query();

results *get_tables(const char *, const char *, const char *);
results *get_columns(const char *, const char *, const char *);
results *db_list_schemas(const char *);
results *db_list_tables(const char *);
results *db_list_columns(const char *);

results *db_autocommit(int);

#endif
