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

static int list_dsns(SQLHENV, SQLUSMALLINT);
static void time_taken(struct timeval *);
static results *fetch_results(SQLHSTMT, struct timeval);
static resultset *fetch_resultset(SQLHSTMT, buffer *);
static row *fetch_row(SQLHSTMT, buffer *, int);

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

void list_all_dsns()
{
	SQLHENV env = alloc_env();

	puts(_("User Data Sources\n-----------------"));
	if(!list_dsns(env, SQL_FETCH_FIRST_USER)) puts(_("(none)"));
	puts(_("\nSystem Data Sources\n-------------------"));
	if(!list_dsns(env, SQL_FETCH_FIRST_SYSTEM)) puts(_("(none)"));
}

static int list_dsns(SQLHENV env, SQLUSMALLINT dir)
{
	SQLCHAR dsn_buf[256], des_buf[256];
	SQLSMALLINT dsn_len, des_len;
	int n = 0;

	while(SQLDataSources(env, dir,
			     dsn_buf, sizeof(dsn_buf), &dsn_len,
			     des_buf, sizeof(des_buf), &des_len) == SQL_SUCCESS) {

		n++;
		printf("%s (%s)\n", dsn_buf, des_buf);
		dir = SQL_FETCH_NEXT;
	}

	return n;
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
	char buf[2];
	SQLRETURN r;

	r = SQLGetInfo(conn, SQL_CATALOG_NAME, buf, 2, 0);
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

static void time_taken(struct timeval *time_taken)
{
	struct timeval end_time;

	gettimeofday(&end_time, 0);

	time_taken->tv_sec  = end_time.tv_sec  - time_taken->tv_sec;
	time_taken->tv_usec = end_time.tv_usec - time_taken->tv_usec;
}
