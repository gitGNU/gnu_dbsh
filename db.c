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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "buffer.h"
#include "db.h"
#include "err.h"
#include "results.h"

#define report_error(t, h, r, f) _report_error(t, h, r, f, __FILE__, __LINE__)

extern const char *dsn, *user, *pass;
extern SQLHDBC conn;

SQLHSTMT *current_statement;
pthread_mutex_t cs_lock = PTHREAD_MUTEX_INITIALIZER;

static void time_taken(struct timeval *);
static results *fetch_results(SQLHSTMT, struct timeval);
static resultset *fetch_resultset(SQLHSTMT, buffer *);
static row *fetch_row(SQLHSTMT, buffer *, int);
static char *get_current_catalog(SQLHDBC);
static void parse_catalog_spec(SQLHDBC, char *, char **, char **);
static void parse_qualified_table(char *, char **, char **);

static void _report_error(SQLSMALLINT type, SQLHANDLE handle, SQLRETURN r,
			  const char *fallback, const char *file, int line)
{
	SQLCHAR message[256];
	SQLCHAR state[6];
	SQLINTEGER code;
	SQLSMALLINT i;
	int success = 0;

	// TODO: only include file and line in debug mode
	printf(_("Error at %s line %d:\n"), file, line);

	for(i = 1;; i++) {
		if(SQL_SUCCEEDED(SQLGetDiagRec(type, handle, i, state, &code, message, 256, 0))) {
			success = 1;
			printf(_("%s (SQLSTATE %s, error %ld)\n"), message, state, code);
		} else break;
	}

	if(!success) {
		fputs(fallback, stdout);
		fputs(_(" (return code was "), stdout);

		switch(r) {
		case SQL_ERROR:
			fputs("SQL_ERROR", stdout);
			break;
		case SQL_INVALID_HANDLE:
			fputs("SQL_INVALID_HANDLE", stdout);
			break;
		case SQL_STILL_EXECUTING:
			fputs("SQL_STILL_EXECUTING", stdout);
			break;
		case SQL_NEED_DATA:
			fputs("SQL_NEED_DATA", stdout);
			break;
		case SQL_NO_DATA:
			fputs("SQL_NO_DATA", stdout);
			break;
		default:
			printf("%ld", (long) r);
		}

		fputs(")\n", stdout);
	}
}

static SQLHENV alloc_env()
{
	static SQLHENV env = {0};
	SQLRETURN r;

	if(!env) {

		r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
		if(!SQL_SUCCEEDED(r)) err_fatal(_("Failed to allocate environment handle\n"));

		r = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
		if(!SQL_SUCCEEDED(r)) {
			SQLFreeHandle(SQL_HANDLE_ENV, env);
			err_fatal(_("Failed to set ODBC version to 3\n"));
		}

	}

	return env;
}

results *db_drivers_and_dsns()
{
	SQLHENV env;
	results *res;
	resultset *s;
	SQLUSMALLINT dir;
	SQLCHAR buf1[256], buf2[256];

	env = alloc_env();
	res = results_alloc();

	s = results_add_set(res);
	resultset_set_cols(s, 1, _("Driver"));
	while(SQL_SUCCEEDED(SQLDrivers(env, SQL_FETCH_NEXT,
				       buf1, sizeof(buf1), 0, 0, 0, 0)))
		resultset_add_row(s, buf1);

	s = results_add_set(res);
	resultset_set_cols(s, 3, _("DSN"), _("Type"), _("Description"));

	dir = SQL_FETCH_FIRST_USER;
	while(SQL_SUCCEEDED(SQLDataSources(env, dir,
					   buf1, sizeof(buf1), 0,
					   buf2, sizeof(buf2), 0))) {
		resultset_add_row(s, buf1, _("user"), buf1);
		dir = SQL_FETCH_NEXT;
	}

	dir = SQL_FETCH_FIRST_SYSTEM;
	while(SQL_SUCCEEDED(SQLDataSources(env, dir,
					   buf1, sizeof(buf1), 0,
					   buf2, sizeof(buf2), 0))) {
		resultset_add_row(s, buf1, _("system"), buf1);
		dir = SQL_FETCH_NEXT;
	}

	return res;
}

SQLHDBC db_connect()
{
	SQLHENV env;
	SQLHDBC conn;
	SQLRETURN r;

	env = alloc_env();

	r = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	if(!SQL_SUCCEEDED(r)) err_fatal(_("Failed to allocate connection handle\n"));


	if(!strchr(dsn, '=') || user) {
		r = SQLConnect(conn,
			       (SQLCHAR *) dsn, SQL_NTS,
			       (SQLCHAR *) user, SQL_NTS,
			       (SQLCHAR *) pass, SQL_NTS);
	} else {
		r = SQLDriverConnect(conn, 0, (SQLCHAR *) dsn, SQL_NTS,
				     0, 0, 0, SQL_DRIVER_NOPROMPT);
	}


	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("Failed to connect"));
		SQLFreeHandle(SQL_HANDLE_DBC, conn);
		return 0;
	}

	printf(_("Connected to %s\n"), dsn);

	return conn;
}

void db_reconnect()
{
	SQLHDBC newconn;

	if((newconn = db_connect(dsn, user, pass))) {
		SQLDisconnect(conn);
		SQLFreeHandle(SQL_HANDLE_DBC, conn);
		conn = newconn;
	}
}

SQLSMALLINT db_info(SQLHDBC conn, SQLUSMALLINT type, char *buf, int len)
{
	SQLRETURN r;
	SQLSMALLINT l;

	r = SQLGetInfo(conn, type, buf, len, &l);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("SQLGetInfo() failed"));
		strncpy(buf, _("(unknown)"), len);
		buf[len - 1] = 0;
		l = strlen(_("(unknown)"));
	}

	return l;
}

SQLINTEGER db_conn_attr(SQLHDBC conn, SQLINTEGER attr, char *buf, int len)
{
	SQLRETURN r;
	SQLINTEGER l;

	r = SQLGetConnectAttr(conn, attr, buf, len, &l);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("SQLGetConnectAttr() failed"));
		strncpy(buf, _("(unknown)"), len);
		buf[len - 1] = 0;
		l = strlen(_("(unknown)"));
	}

	return l;
}

#define ADD_INFO(t, n) db_info(conn, t, buf, 256); results_add_row(res, n, buf)

results *db_conn_details(SQLHDBC conn)
{
	results *res;
	char buf[256];
	SQLUINTEGER i;
	const char *s;

	res = results_single_alloc();

	results_set_cols(res, 2, _("name"), _("value"));

	// Connection info
	ADD_INFO(SQL_DATA_SOURCE_NAME, _("DSN"));
	ADD_INFO(SQL_SERVER_NAME,      _("Server"));
	ADD_INFO(SQL_DBMS_NAME,        _("DBMS name"));
	ADD_INFO(SQL_DBMS_VER,         _("DBMS version"));
	ADD_INFO(SQL_USER_NAME,        _("DBMS user"));

	// Driver info
	ADD_INFO(SQL_DRIVER_NAME, _("Driver"));
	ADD_INFO(SQL_DRIVER_VER,  _("Driver version"));
	ADD_INFO(SQL_ODBC_VER,    _("ODBC version"));

	SQLGetInfo(conn, SQL_ODBC_INTERFACE_CONFORMANCE, &i, 0, 0);
	switch(i) {
	case SQL_OIC_CORE:
		s = "Core";
		break;
	case SQL_OIC_LEVEL1:
		s = "Level 1";
		break;
	case SQL_OIC_LEVEL2:
		s = "Level 2";
		break;
	default:
		s = _("(unknown)");
	}
	results_add_row(res, _("ODBC compliance"), s);

	SQLGetInfo(conn, SQL_SQL_CONFORMANCE, &i, 0, 0);
	switch(i) {
	case SQL_SC_SQL92_ENTRY:
		s = "Entry level";
		break;
	case SQL_SC_FIPS127_2_TRANSITIONAL:
		s = "FIPS 127-2 transitional level";
		break;
	case SQL_SC_SQL92_INTERMEDIATE:
		s = "Intermediate level";
		break;
	case SQL_SC_SQL92_FULL:
		s = "Full level";
		break;
	default:
		s = _("(unknown)");
	}
	results_add_row(res, _("SQL-92 compliance"), s);

	// Driver Manager info
	ADD_INFO(SQL_DM_VER,         _("Driver Manager version"));
	ADD_INFO(SQL_XOPEN_CLI_YEAR, _("X/Open CLI compliance"));

	// Useless but potentially interesting info
	ADD_INFO(SQL_CATALOG_TERM,   _("Catalog term"));

	ADD_INFO(SQL_SCHEMA_TERM,    _("Schema term"));
	ADD_INFO(SQL_TABLE_TERM,     _("Table term"));
	ADD_INFO(SQL_PROCEDURE_TERM, _("Procedure term"));


	return res;
}

int db_supports_catalogs(SQLHDBC conn)
{
	char buf[4];
	SQLRETURN r;

	r = SQLGetInfo(conn, SQL_CATALOG_NAME, buf, 4, 0);
	if(!SQL_SUCCEEDED(r)) return 0;

	return (buf[0] == 'Y');
}

results *execute_query(SQLHDBC conn, const char *buf, int buflen)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		puts(_("Failed to allocate statement handle"));
		return 0;
	}

	pthread_mutex_lock(&cs_lock);
	current_statement = &st;
	pthread_mutex_unlock(&cs_lock);

	gettimeofday(&taken, 0);

	if(SQLExecDirect(st, (SQLCHAR *) buf, buflen) == SQL_ERROR) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to execute statement"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return 0;
	}

	time_taken(&taken);

	return fetch_results(st, taken);
}

static results *fetch_results(SQLHSTMT st, struct timeval time_taken)
{
	results *res;
	resultset **sp;
	buffer *buf;
	SQLINTEGER i;
	SQLSMALLINT reqlen;
	SQLRETURN r;

	res = results_alloc();
	res->time_taken = time_taken;

	buf = buffer_alloc(1024);

	SQLGetDiagField(SQL_HANDLE_STMT, st, 0, SQL_DIAG_NUMBER, &(res->nwarnings), 0, 0);
	if(res->nwarnings) {
		if(!(res->warnings = calloc(res->nwarnings, sizeof(char *)))) err_system();
		for(i = 0; i < res->nwarnings; i++) {
			SQLGetDiagField(SQL_HANDLE_STMT, st, i + 1,
					SQL_DIAG_MESSAGE_TEXT, buf->buf, buf->len, &reqlen);

			if(reqlen + 1 > buf->len) {
				buffer_realloc(buf, reqlen + 1);
				SQLGetDiagField(SQL_HANDLE_STMT, st, i + 1,
						SQL_DIAG_MESSAGE_TEXT, buf->buf, buf->len, 0);
			}

			if(!(res->warnings[i] = strdup2wcs(buf->buf))) err_system();
		}
	}

	sp = &res->sets;

	do {
		*sp = fetch_resultset(st, buf);
		sp = &(*sp)->next;
		r = SQLMoreResults(st);
	} while(SQL_SUCCEEDED(r));

	buffer_free(buf);

	pthread_mutex_lock(&cs_lock);
	current_statement = 0;
	pthread_mutex_unlock(&cs_lock);

	SQLFreeHandle(SQL_HANDLE_STMT, st);

	return res;
}

static resultset *fetch_resultset(SQLHSTMT st, buffer *buf)
{
	resultset *res;
	SQLRETURN r;
	SQLSMALLINT i;
	SQLSMALLINT reqlen;
	row **rowp;

	res = resultset_alloc();

	r = SQLNumResultCols(st, &(res->ncols));
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve number of columns"));
		resultset_free(res);
		return 0;
	}

	if(!res->ncols) {  // non-SELECT
		r = SQLRowCount(st, &(res->nrows));
		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve rows affected"));
			resultset_free(res);
			return 0;
		}

		return res;
	}

	if(!(res->cols = calloc(res->ncols, sizeof(char *)))) err_system();

	for(i = 0; i < res->ncols; i++) {
		SQLSMALLINT type;
		SQLUINTEGER size;
		SQLSMALLINT digits;
		SQLSMALLINT nullable;

		// TODO: is there a way to just get the name?
		r = SQLDescribeCol(st, i + 1, (SQLCHAR *) buf->buf, buf->len, &reqlen,
				   &type, &size, &digits, &nullable);

		if(reqlen + 1 > buf->len) {
			buffer_realloc(buf, reqlen + 1);
			r = SQLDescribeCol(st, i + 1, (SQLCHAR *) buf->buf, buf->len, 0,
					   &type, &size, &digits, &nullable);
		}

		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve column data"));
			resultset_free(res);
			return 0;
		}

		if(!(res->cols[i] = strdup2wcs(buf->buf))) err_system();
	}

	for(rowp = &(res->rows);;) {
		*rowp = fetch_row(st, buf, res->ncols);
		if(!*rowp) break;
		rowp = &(*rowp)->next;
		res->nrows++;
	}

	return res;
}

static row *fetch_row(SQLHSTMT st, buffer *buf, int ncols)
{
	row *row;
	SQLRETURN r;
	SQLSMALLINT i;
	SQLINTEGER reqlen;

	r = SQLFetch(st);
	if(!SQL_SUCCEEDED(r)) return 0;

	row = results_row_alloc(ncols);

	for(i = 0; i < ncols; i++) {
		r = SQLGetData(st, i + 1, SQL_C_CHAR, buf->buf, buf->len, &reqlen);

		if(reqlen + 1 > buf->len) {
			buffer_realloc(buf, reqlen + 1);
			r = SQLGetData(st, i + 1, SQL_C_CHAR, buf->buf, buf->len, 0);
		}

		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to fetch row"));
			results_row_free(row, ncols);
			return 0;
		}

		if(reqlen != SQL_NULL_DATA &&
		   !(row->data[i] = strdup2wcs(buf->buf))) err_system();
	}

	return row;
}

void db_cancel_query()
{
	SQLRETURN r;

	pthread_mutex_lock(&cs_lock);

	if(current_statement) {
		r = SQLCancel(*current_statement);
		if(!SQL_SUCCEEDED(r)) report_error(SQL_HANDLE_STMT, *current_statement, r, "Failed to cancel query");
	}

	pthread_mutex_unlock(&cs_lock);
}

results *get_tables(SQLHDBC conn, const char *catalog,
		       const char *schema, const char *table)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;


	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to allocate statement handle"));
		return 0;
	}

	gettimeofday(&taken, 0);
	current_statement = st;
	r = SQLTables(st,
		      (SQLCHAR *) catalog, SQL_NTS,
		      (SQLCHAR *) schema, SQL_NTS,
		      (SQLCHAR *) table, SQL_NTS,
		      (SQLCHAR *) 0, 0);
	time_taken(&taken);

	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to list tables"));
		return 0;
	}

	return fetch_results(st, taken);
}

results *get_columns(SQLHDBC conn, const char *catalog,
			const char *schema, const char *table)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to allocate statement handle"));
		return 0;
	}

	gettimeofday(&taken, 0);
	current_statement = st;

	r = SQLColumns(st,
		      (SQLCHAR *) catalog, SQL_NTS,
		      (SQLCHAR *) schema, SQL_NTS,
		      (SQLCHAR *) table, SQL_NTS,
		      (SQLCHAR *) "%", SQL_NTS);
	time_taken(&taken);

	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to list columns"));
		return 0;
	}

	return fetch_results(st, taken);
}

results *db_list_tables(SQLHDBC conn, const char *spec)
{
	char *catalog, *schema;
	char *buf;
	results *res;

	catalog = 0;
	schema = 0;
	buf = 0;

	if(db_supports_catalogs(conn)) {
		if(spec) {
			if(!(buf = strdup(spec))) err_system();
			parse_catalog_spec(conn, buf, &catalog, &schema);
		} else {
			buf = get_current_catalog(conn);
			catalog = buf;
		}
	} else schema = (char *) spec;

	res = get_tables(conn, catalog, schema, 0);
	if(buf) free(buf);
	return res;
}

results *db_list_columns(SQLHDBC conn, const char *spec)
{
	char *catalog, *schema, *table;
	char *dspec, *buf;
	results *res;

	if(!(dspec = strdup(spec))) err_system();
	buf = 0;

	if(db_supports_catalogs(conn)) {

		parse_catalog_spec(conn, dspec, &catalog, &table);

		if(table) parse_qualified_table(table, &schema, &table);
		else {
			parse_qualified_table(catalog, &schema, &table);
			buf = get_current_catalog(conn);
			catalog = buf;
		}
	} else {
		catalog = 0;
		parse_qualified_table(dspec, &schema, &table);
	}

	res = get_columns(conn, catalog, schema, table);
	free(dspec);
	if(buf) free(buf);
	return res;
}

static char *get_current_catalog(SQLHDBC conn)
{
	SQLSMALLINT buflen;
	char *buf;
	SQLRETURN r;

	r = SQLGetInfo(conn, SQL_MAX_CATALOG_NAME_LEN, &buflen, 0, 0);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r,
			     _("Failed to get max catalog name len"));
		buflen = 128;
	}
	if(!buflen) buflen = 128;

	if(!(buf = malloc(buflen))) err_system();

	r = SQLGetConnectAttr(conn, SQL_ATTR_CURRENT_CATALOG, buf, buflen, 0);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r,
			     _("Failed to get current catalog"));
		*buf = 0;
	}

	return buf;
}

static void parse_catalog_spec(SQLHDBC conn, char *spec,
			     char **catalog, char **schema)
{
	SQLUSMALLINT catloc;
	char sep[8], *p, *q;
	SQLSMALLINT seplen;
	SQLRETURN r;

	*catalog = spec;
	*schema = 0;


	r = SQLGetInfo(conn, SQL_CATALOG_LOCATION, &catloc, 0, 0);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("Failed to get catalog location"));
		catloc = SQL_CL_START;
	}

	r = SQLGetInfo(conn, SQL_CATALOG_NAME_SEPARATOR, sep, 8, &seplen);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("Failed to get catalog name separator"));
		strcpy(sep, ".");
	}

	if(catloc == SQL_CL_START) {
		p = strstr(spec, sep);
		if(p) {
			*schema = p + seplen;
			*p = 0;
		}
	} else {
		q = 0;
		while((p = strstr(spec, sep))) q = p;

		if(q) {
			*catalog = q + seplen;
			*schema = spec;
			*q = 0;
		}
	}
}

static void parse_qualified_table(char *s, char **schema, char **table)
{
	char *p;

	if((p = strchr(s, '.'))) {
		*schema = s;
		*table = p + 1;
		*p = 0;
	} else {
		*schema = 0;
		*table = s;
	}
}

static void time_taken(struct timeval *time_taken)
{
	struct timeval end_time;

	gettimeofday(&end_time, 0);

	time_taken->tv_sec  = end_time.tv_sec  - time_taken->tv_sec;
	time_taken->tv_usec = end_time.tv_usec - time_taken->tv_usec;
}
