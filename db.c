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
#include "parser.h"
#include "results.h"

#define report_error(t, h, r, f) _report_error(t, h, r, f, __FILE__, __LINE__)

extern const char *dsn, *user, *pass;
SQLHDBC conn;

SQLHSTMT *current_statement;
pthread_mutex_t cs_lock = PTHREAD_MUTEX_INITIALIZER;


static void set_current_statement(SQLHSTMT *);
static void fetch_warnings(results *, SQLSMALLINT, SQLHANDLE);
static void fetch_results(results *, SQLHSTMT);
static void fetch_resultset(results *, SQLHSTMT, buffer *);
static int fetch_row(results *, SQLHSTMT, buffer *);
static char *get_current_catalog();
static void parse_catalog_spec(char *, char **, char **);
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
		if(!SQL_SUCCEEDED(r)) err_fatal(_("Failed to allocate environment handle"));

		r = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
		if(!SQL_SUCCEEDED(r)) {
			SQLFreeHandle(SQL_HANDLE_ENV, env);
			err_fatal(_("Failed to set ODBC version to 3"));
		}

	}

	return env;
}

results *db_drivers_and_dsns()
{
	SQLHENV env;
	results *res;
	SQLUSMALLINT dir;
	SQLCHAR buf1[256], buf2[256];

	env = alloc_env();

	res = res_alloc();
	res_set_cols(res, 1, _("Driver"));
	while(SQL_SUCCEEDED(SQLDrivers(env, SQL_FETCH_NEXT,
				       buf1, sizeof(buf1), 0, 0, 0, 0)))
		res_add_row(res, buf1);

	res_add_set(res);
	res_set_cols(res, 3, _("DSN"), _("Type"), _("Description"));

	dir = SQL_FETCH_FIRST_USER;
	while(SQL_SUCCEEDED(SQLDataSources(env, dir,
					   buf1, sizeof(buf1), 0,
					   buf2, sizeof(buf2), 0))) {
		res_add_row(res, buf1, _("user"), buf1);
		dir = SQL_FETCH_NEXT;
	}

	dir = SQL_FETCH_FIRST_SYSTEM;
	while(SQL_SUCCEEDED(SQLDataSources(env, dir,
					   buf1, sizeof(buf1), 0,
					   buf2, sizeof(buf2), 0))) {
		res_add_row(res, buf1, _("system"), buf1);
		dir = SQL_FETCH_NEXT;
	}

	return res;
}

static SQLHDBC connect()
{
	SQLHENV env;
	SQLHDBC conn;
	SQLSMALLINT ldsn, luser, lpass;
	SQLRETURN r;

	env = alloc_env();

	r = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	if(!SQL_SUCCEEDED(r)) err_fatal(_("Failed to allocate connection handle"));

	ldsn =  dsn  ? SQL_NTS : 0;
	luser = user ? SQL_NTS : 0;
	lpass = pass ? SQL_NTS : 0;

	if(!strchr(dsn, '=') || user) {
		r = SQLConnect(conn,
			       (SQLCHAR *) dsn, ldsn,
			       (SQLCHAR *) user, luser,
			       (SQLCHAR *) pass, lpass);
	} else {
		r = SQLDriverConnect(conn, 0, (SQLCHAR *) dsn, ldsn,
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

int db_connect()
{
	conn = connect();
	return conn ? 1 : 0;
}

void db_reconnect()
{
	SQLHDBC newconn;

	if((newconn = connect())) {
		SQLDisconnect(conn);
		SQLFreeHandle(SQL_HANDLE_DBC, conn);
		conn = newconn;
	}
}

void db_close()
{
	SQLDisconnect(conn);
	SQLFreeHandle(SQL_HANDLE_DBC, conn);
	SQLFreeHandle(SQL_HANDLE_ENV, alloc_env());
}

SQLSMALLINT db_info(SQLUSMALLINT type, char *buf, int len)
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

SQLINTEGER db_conn_attr(SQLINTEGER attr, char *buf, int len)
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

#define ADD_INFO(t, n) db_info(t, buf, 256); res_add_row(res, n, buf)

results *db_conn_details()
{
	results *res;
	char buf[256];
	SQLUINTEGER i;
	const char *s;

	res = res_alloc();

	res_set_cols(res, 2, _("name"), _("value"));

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
	res_add_row(res, _("ODBC compliance"), s);

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
	res_add_row(res, _("SQL-92 compliance"), s);

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

results *execute_query(const char *buf, int buflen, parsed_line *params)
{
	SQLHSTMT st;
	results *res;
	int i, l;
	SQLRETURN r;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		puts(_("Failed to allocate statement handle"));
		return 0;
	}

	set_current_statement(&st);

	res = res_alloc();
	res_start_timer(res);

	r = SQLPrepare(st, (SQLCHAR *) buf, buflen);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to prepare statement"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		res_free(res);
		return 0;
	} else if(r == SQL_SUCCESS_WITH_INFO) {
		fetch_warnings(res, SQL_HANDLE_STMT, st);
	}

	for(i = 0; i < params->nchunks; i++) {
		l = strlen(params->chunks[i]);

		r = SQLBindParameter(st, i + 1, SQL_PARAM_INPUT, SQL_C_CHAR,
				     SQL_CHAR, l, 0, params->chunks[i], l, 0);
		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to bind parameter"));
			SQLFreeHandle(SQL_HANDLE_STMT, st);
			res_free(res);
			return 0;
		} else if(r == SQL_SUCCESS_WITH_INFO) {
			fetch_warnings(res, SQL_HANDLE_STMT, st);
		}
	}

	r = SQLExecute(st);
	if(!SQL_SUCCEEDED(r) && r != SQL_NO_DATA) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to execute statement"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		res_free(res);
		return 0;
	} else if(r == SQL_SUCCESS_WITH_INFO) {
		fetch_warnings(res, SQL_HANDLE_STMT, st);
	}

	fetch_results(res, st);
	res_stop_timer(res);

	return res;
}

static void set_current_statement(SQLHSTMT *stp)
{
	pthread_mutex_lock(&cs_lock);
	current_statement = stp;
	pthread_mutex_unlock(&cs_lock);
}

static void fetch_warnings(results *res, SQLSMALLINT type, SQLHANDLE h)
{
	SQLINTEGER n, i;
	buffer *buf;
	SQLSMALLINT reqlen;

	SQLGetDiagField(SQL_HANDLE_STMT, h, 0, SQL_DIAG_NUMBER, &n, 0, 0);
	if(!n) return;

	buf = buffer_alloc(1024);

	for(i = 0; i < n; i++) {
		SQLGetDiagField(SQL_HANDLE_STMT, h, i + 1, SQL_DIAG_MESSAGE_TEXT,
				buf->buf, buf->len, &reqlen);

		if(reqlen + 1 > buf->len) {
			buffer_realloc(buf, reqlen + 1);
			SQLGetDiagField(SQL_HANDLE_STMT, h, i + 1, SQL_DIAG_MESSAGE_TEXT,
					buf->buf, buf->len, 0);

		}

		res_add_warning(res, buf->buf);
	}

	buffer_free(buf);
}

void fetch_results(results *res, SQLHSTMT st)
{
	buffer *buf;
	SQLRETURN r;

	buf = buffer_alloc(1024);

	for(;;) {
		fetch_resultset(res, st, buf);

		r = SQLMoreResults(st);

		if(r == SQL_NO_DATA) {
			break;
		} else if(!SQL_SUCCEEDED(r)) {
			fetch_warnings(res, SQL_HANDLE_STMT, st);
			break;
		} else if(r == SQL_SUCCESS_WITH_INFO) {
			fetch_warnings(res, SQL_HANDLE_STMT, st);
		}

		res_add_set(res);
	}

	buffer_free(buf);

	set_current_statement(0);
	SQLFreeHandle(SQL_HANDLE_STMT, st);
}

void fetch_resultset(results *res, SQLHSTMT st, buffer *buf)
{
	SQLSMALLINT ncols, i, reqlen;
	SQLLEN nrows;
	SQLRETURN r;

	r = SQLNumResultCols(st, &ncols);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve number of columns"));
		return;
	}
	res_set_ncols(res, ncols);

	if(!ncols) {  // non-SELECT
		r = SQLRowCount(st, &nrows);
		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve rows affected"));
			return;
		}

		res_set_nrows(res, nrows);
		return;
	}

	for(i = 0; i < ncols; i++) {
		SQLSMALLINT type;
		SQLUINTEGER size;
		SQLSMALLINT digits;
		SQLSMALLINT nullable;

		r = SQLDescribeCol(st, i + 1, (SQLCHAR *) buf->buf, buf->len, &reqlen,
				   &type, &size, &digits, &nullable);

		if(reqlen + 1 > buf->len) {
			buffer_realloc(buf, reqlen + 1);
			r = SQLDescribeCol(st, i + 1, (SQLCHAR *) buf->buf, buf->len, 0,
					   &type, &size, &digits, &nullable);
		}

		if(!SQL_SUCCEEDED(r)) {
			report_error(SQL_HANDLE_STMT, st, r, _("Failed to retrieve column data"));
			return;
		}

		res_set_col(res, i, buf->buf);
	}


	while(fetch_row(res, st, buf));
}

static int fetch_row(results *res, SQLHSTMT st, buffer *buf)
{
	SQLRETURN r;
	SQLSMALLINT i;
	SQLINTEGER reqlen, offset;


	r = SQLFetch(st);
	if(!SQL_SUCCEEDED(r)) return 0;

	res_new_row(res);

	for(i = 0; i < res_get_ncols(res); i++) {
		offset = 0;

		for(;;) {
			r = SQLGetData(st, i + 1, SQL_C_CHAR,
				       buf->buf + offset, buf->len - offset,
				       &reqlen);

			if(r == SQL_NO_DATA) break;
			else if(!SQL_SUCCEEDED(r)) {
				report_error(SQL_HANDLE_STMT, st, r, _("Failed to fetch row"));
				return 0;
			}

			if(reqlen == SQL_NULL_DATA) break;
			else if(reqlen == SQL_NO_TOTAL) reqlen = buf->len + 1024;  // guess

			if(reqlen > 0 && reqlen + 1 > buf->len) {
				offset = buf->len - 1;
				buffer_realloc(buf, reqlen + 1);
			} else break;
		}

		if(reqlen != SQL_NULL_DATA) res_set_value(res, i, buf->buf);
	}

	return 1;
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

results *get_tables(const char *catalog,
		       const char *schema, const char *table)
{
	SQLHSTMT st;
	SQLRETURN r;
	results *res;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to allocate statement handle"));
		return 0;
	}

	set_current_statement(&st);

	res = res_alloc();
	res_start_timer(res);

	current_statement = st;
	r = SQLTables(st,
		      (SQLCHAR *) catalog, SQL_NTS,
		      (SQLCHAR *) schema, SQL_NTS,
		      (SQLCHAR *) table, SQL_NTS,
		      (SQLCHAR *) 0, 0);

	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to list tables"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		res_free(res);
		return 0;
	} else if(r == SQL_SUCCESS_WITH_INFO) {
		fetch_warnings(res, SQL_HANDLE_STMT, st);
	}

	fetch_results(res, st);
	res_stop_timer(res);

	return res;
}

results *get_columns(const char *catalog,
			const char *schema, const char *table)
{
	SQLHSTMT st;
	results *res;
	SQLRETURN r;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to allocate statement handle"));
		return 0;
	}

	set_current_statement(&st);

	res = res_alloc();
	res_start_timer(res);

	r = SQLColumns(st,
		      (SQLCHAR *) catalog, SQL_NTS,
		      (SQLCHAR *) schema, SQL_NTS,
		      (SQLCHAR *) table, SQL_NTS,
		      (SQLCHAR *) "%", SQL_NTS);

	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_STMT, st, r, _("Failed to list columns"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		res_free(res);
		return 0;
	} else if(r == SQL_SUCCESS_WITH_INFO) {
		fetch_warnings(res, SQL_HANDLE_STMT, st);
	}

	fetch_results(res, st);
	res_stop_timer(res);

	return res;
}

results *db_list_tables(const char *spec)
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
			parse_catalog_spec(buf, &catalog, &schema);
		} else {
			buf = get_current_catalog();
			catalog = buf;
		}
	} else schema = (char *) spec;

	res = get_tables(catalog, schema, 0);
	if(buf) free(buf);
	return res;
}

results *db_list_columns(const char *spec)
{
	char *catalog, *schema, *table;
	char *dspec, *buf;
	results *res;

	if(!(dspec = strdup(spec))) err_system();
	buf = 0;

	if(db_supports_catalogs(conn)) {

		parse_catalog_spec(dspec, &catalog, &table);

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

	res = get_columns(catalog, schema, table);
	free(dspec);
	if(buf) free(buf);
	return res;
}

static char *get_current_catalog()
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

static void parse_catalog_spec(char *spec, char **catalog, char **schema)
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

results *db_autocommit(int change)
{
	results *res;
	SQLUINTEGER state;
	const char *text;
	SQLRETURN r;

	res = res_alloc();
	res_set_cols(res, 1, _("Autocommit state"));

	if(change) {
		state = change > 0 ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
		r = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) state, 0);
		if(r == SQL_SUCCESS_WITH_INFO || r == SQL_ERROR)
			fetch_warnings(res, SQL_HANDLE_DBC, conn);
	}

	r = SQLGetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT, &state, 0, 0);
	if(r == SQL_SUCCESS_WITH_INFO || r == SQL_ERROR)
		fetch_warnings(res, SQL_HANDLE_DBC, conn);

	if(SQL_SUCCEEDED(r)) text = (state == SQL_AUTOCOMMIT_ON) ? _("On") : _("Off");
	else text = _("(unknown)");

	res_add_row(res, text);

	return res;
}

results *db_endtran(int commit)
{
	results *res;
	SQLRETURN r;

	res = res_alloc();
	res_set_nrows(res, -1);

	r = SQLEndTran(SQL_HANDLE_DBC, conn, commit ? SQL_COMMIT : SQL_ROLLBACK);
	if(!SQL_SUCCEEDED(r)) {
		report_error(SQL_HANDLE_DBC, conn, r, _("Failed to execute statement"));
		res_free(res);
		return 0;
	} else if(r == SQL_SUCCESS_WITH_INFO) {
		fetch_warnings(res, SQL_HANDLE_DBC, conn);
	}

	return res;
}

