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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "err.h"
#include "results.h"

#define SUCCESS(r) (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO)
#define report_error(t, h) _report_error(t, h,  __FILE__, __LINE__)

SQLHSTMT *current_statement;

static int list_dsns(SQLHENV, SQLUSMALLINT);
static void time_taken(struct timeval *);
static results *fetch_results(SQLHSTMT, struct timeval);
static results *fetch_resultset(SQLHSTMT, struct timeval);

static int _report_error(SQLSMALLINT type, SQLHANDLE handle, const char *file, int line)
{
	SQLRETURN r;
	SQLCHAR message[256];
	SQLCHAR state[6];
	SQLINTEGER code;
	SQLSMALLINT i;
	int retval = 0;

	// TODO: only include file and line in debug mode
	printf(_("Error at %s line %d:\n"), file, line);

	for(i = 1;; i++) {
		r = SQLGetDiagRec(type, handle, i, state, &code, message, 256, 0);

		if(SUCCESS(r)) {
			retval = 1;
			printf(_("%s (SQLSTATE %s, error %ld)\n"), message, state, code);
		} else break;
	}

	return retval;
}

static SQLHENV alloc_env()
{
	static SQLHENV env = {0};
	SQLRETURN r;

	if(!env) {

		r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
		if(!SUCCESS(r)) err_fatal(_("Failed to allocate environment handle\n"));

		r = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
		if(!SUCCESS(r)) {
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

SQLHDBC db_connect(const char *dsn, const char *user, const char *pass)
{
	SQLHENV env;
	SQLHDBC conn;
	SQLRETURN r;

	env = alloc_env();

	r = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	if(!SUCCESS(r)) err_fatal(_("Failed to allocate connection handle\n"));

	r = SQLConnect(conn,
		       (SQLCHAR *) dsn, SQL_NTS,
		       (SQLCHAR *) user, SQL_NTS,
		       (SQLCHAR *) pass, SQL_NTS);
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_DBC, conn))
			printf(_("Failed to connect to %s\n"), dsn);
		exit(1);
	}

	printf(_("Connected to %s\n"), dsn);

	return conn;
}

int db_reconnect(SQLHDBC *conn, const char *pass)
{
	char dsn[256], user[256];
	SQLHENV env;
	SQLHDBC newconn;
	SQLRETURN r;

	db_info(*conn, SQL_DATA_SOURCE_NAME, dsn, 256);
	db_info(*conn, SQL_USER_NAME, user, 256);

	env = alloc_env();

	r = SQLAllocHandle(SQL_HANDLE_DBC, env, &newconn);
	if(!SUCCESS(r)) err_fatal(_("Failed to allocate connection handle\n"));

	r = SQLConnect(newconn,
		       (SQLCHAR *) dsn, SQL_NTS,
		       (SQLCHAR *) user, SQL_NTS,
		       (SQLCHAR *) pass, SQL_NTS);

	if(SUCCESS(r)) {
		printf(_("Connected to %s\n"), dsn);
		SQLDisconnect(*conn);
		SQLFreeHandle(SQL_HANDLE_DBC, *conn);
		*conn = newconn;
		return 0;

	} else {
		if(!report_error(SQL_HANDLE_DBC, newconn))
			printf(_("Failed to connect to %s\n"), dsn);
		SQLFreeHandle(SQL_HANDLE_DBC, newconn);
		return 1;
	}
}

SQLSMALLINT db_info(SQLHDBC conn, SQLUSMALLINT type, char *buf, int len)
{
	SQLRETURN r;
	SQLSMALLINT l;

	r = SQLGetInfo(conn, type, buf, len, &l);
	if(!SUCCESS(r)) {
		report_error(SQL_HANDLE_DBC, conn);
		strncpy(buf, "(unknown)", len);
		buf[len - 1] = 0;
	}

	return l;
}

results *db_conn_details(SQLHDBC conn)
{
	results *res;
	char buf[256];

	res = results_alloc();

	results_set_cols(res, 2, _("name"), _("value"));
	results_set_rows(res, 3);

	db_info(conn, SQL_SERVER_NAME, buf, 256);
	res->data[0][0] = strdup(_("Server"));
	res->data[0][1] = strdup(buf);

	db_info(conn, SQL_DBMS_NAME, buf, 256);
	res->data[1][0] = strdup(_("DBMS"));
	res->data[1][1] = strdup(buf);

	db_info(conn, SQL_DBMS_VER, buf, 256);
	res->data[2][0] = strdup(_("Version"));
	res->data[2][1] = strdup(buf);

	return res;
}

results *execute_query(SQLHDBC conn, const char *buf, int buflen)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SUCCESS(r)) {
		puts(_("Failed to allocate statement handle"));
		return 0;
	}

	gettimeofday(&taken, 0);

	current_statement = &st;
	r = SQLExecDirect(st, (SQLCHAR *) buf, buflen);

	time_taken(&taken);

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to execute statement"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return 0;
	}

	return fetch_results(st, taken);
}

static results *fetch_results(SQLHSTMT st, struct timeval time_taken)
{
	results *res, **resp;

	resp = &res;

	do {
		*resp = fetch_resultset(st, time_taken);
		resp = &((*resp)->next);
	} while(SUCCESS(SQLMoreResults(st)));

	current_statement = 0;
	SQLFreeHandle(SQL_HANDLE_STMT, st);

	return res;
}

static results *fetch_resultset(SQLHSTMT st, struct timeval time_taken)
{
	results *res;
	SQLRETURN r;
	SQLSMALLINT i;
	SQLINTEGER j;
	SQLCHAR *buf;
	SQLINTEGER buflen;

	buflen = 1024;
	if(!(buf = malloc(buflen))) err_system();

	res = results_alloc();
	res->time_taken = time_taken;

	SQLGetDiagField(SQL_HANDLE_STMT, st, 0, SQL_DIAG_NUMBER, &(res->nwarnings), 0, 0);
	if(res->nwarnings) {
		if(!(res->warnings = calloc(res->nwarnings, sizeof(char *)))) err_system();
		for(j = 0; j < res->nwarnings; j++) {
			SQLSMALLINT reqlen;
			SQLGetDiagField(SQL_HANDLE_STMT, st, j + 1,
					SQL_DIAG_MESSAGE_TEXT, buf, buflen, &reqlen);

			if(reqlen + 1 > buflen) {
				buflen = reqlen + 1;
				if(!(buf = realloc(buf, buflen))) err_system();

				SQLGetDiagField(SQL_HANDLE_STMT, st, j + 1,
						SQL_DIAG_MESSAGE_TEXT, buf, buflen, 0);
			}

			res->warnings[j] = strdup((char *) buf);
		}
	}

	r = SQLRowCount(st, &(res->nrows));
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to retrieve number of rows"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		results_free(res);
		return 0;
	}

	if(res->nrows == -1) {
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return res;
	}

	r = SQLNumResultCols(st, &(res->ncols));
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to retrieve number of columns"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		results_free(res);
		return 0;
	}

	if(!res->ncols) {
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return res;
	}

	if(!(res->cols = calloc(res->ncols, sizeof(char *)))) err_system();

	for(i = 0; i < res->ncols; i++) {
		SQLSMALLINT reqlen;
		SQLSMALLINT type;
		SQLUINTEGER size;
		SQLSMALLINT digits;
		SQLSMALLINT nullable;

		// TODO: is there a way to just get the name?
		r = SQLDescribeCol(st, i + 1, buf, buflen, &reqlen,
				   &type, &size, &digits, &nullable);

		if(reqlen + 1 > buflen) {
			buflen = reqlen + 1;
			if(!(buf = realloc(buf, buflen))) err_system();

			r = SQLDescribeCol(st, i + 1, buf, buflen, 0,
					   &type, &size, &digits, &nullable);
		}

		if(!SUCCESS(r)) {
			if(!report_error(SQL_HANDLE_STMT, st))
				puts(_("Failed to retrieve column data"));
			SQLFreeHandle(SQL_HANDLE_STMT, st);
			results_free(res);
			return 0;
		}

		res->cols[i] = strdup((char *) buf);
	}

	if(!res->nrows) {
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return res;
	}


	if(!(res->data = calloc(res->nrows, sizeof(char **)))) err_system();
	for(j = 0; j < res->nrows; j++) {
		r = SQLFetch(st);
		if(!SUCCESS(r)) {
			if(!report_error(SQL_HANDLE_STMT, st))
				printf(_("Failed to fetch row %ld (apparently %ld rows)\n"), j, res->nrows);
			SQLFreeHandle(SQL_HANDLE_STMT, st);
			results_free(res);
			return 0;
		}

		if(!(res->data[j] = calloc(res->ncols, sizeof(char *)))) err_system();

		for(i = 0; i < res->ncols; i++) {
			SQLINTEGER reqlen;
			r = SQLGetData(st, i + 1, SQL_C_CHAR, buf, buflen, &reqlen);

			if(reqlen + 1 > buflen) {
				buflen = reqlen + 1;
				if(!(buf = realloc(buf, buflen))) err_system();

				r = SQLGetData(st, i + 1, SQL_C_CHAR, buf, buflen, &reqlen);
			}

			if(!SUCCESS(r)) {
				if(!report_error(SQL_HANDLE_STMT, st))
					printf(_("Failed to fetch column %d from row %ld\n"), i, j);
				SQLFreeHandle(SQL_HANDLE_STMT, st);
				results_free(res);
				return 0;
			}

			if(reqlen != SQL_NULL_DATA) res->data[j][i] = strdup((char *) buf);
		}
	}

	return res;
}

void cancel_query()
{
	SQLRETURN r;

	if(current_statement) {
		r = SQLCancel(*current_statement);
		if(!SUCCESS(r)) report_error(SQL_HANDLE_STMT, *current_statement);
	}
}

results *get_tables(SQLHDBC conn, const char *catalog,
		       const char *schema, const char *table)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;


	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to allocate statement handle"));
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

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to list tables"));
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
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to allocate statement handle"));
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

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			puts(_("Failed to list columns"));
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
