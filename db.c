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

SQLHENV alloc_env()
{
	SQLHENV env;
	SQLRETURN r;


	r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	if(!SUCCESS(r)) err_fatal(_("Failed to allocate environment handle\n"));

	r = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	if(!SUCCESS(r)) {
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		err_fatal(_("Failed to set ODBC version to 3\n"));
	}

	return env;
}

void list_all_dsns()
{
	SQLHENV env = alloc_env();

	printf(_("User Data Sources\n-----------------\n"));
	if(!list_dsns(env, SQL_FETCH_FIRST_USER)) printf(_("(none)\n"));
	printf(_("\nSystem Data Sources\n-------------------\n"));
	if(!list_dsns(env, SQL_FETCH_FIRST_SYSTEM)) printf(_("(none)\n"));
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

SQLHDBC connect_dsn(SQLHENV env, const char *dsn, const char *user, const char *pass)
{
	SQLHDBC conn;
	SQLRETURN r;
	SQLCHAR buf[256];

	r = SQLAllocHandle(SQL_HANDLE_DBC, env, &conn);
	if(!SUCCESS(r)) {
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		err_fatal(_("Failed to allocate connection handle\n"));
	}

	r = SQLConnect(conn,
		       (SQLCHAR *) dsn, SQL_NTS,
		       (SQLCHAR *) user, SQL_NTS,
		       (SQLCHAR *) pass, SQL_NTS);
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_DBC, conn))
			printf(_("Failed to connect to %s\n"), dsn);
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		exit(1);
	}

	printf(_("Connected to %s\n"), dsn);

	r = SQLGetInfo(conn, SQL_SERVER_NAME, buf, 256, 0);
	if(SUCCESS(r)) printf(_("Server:  %s\n"), buf);

	r = SQLGetInfo(conn, SQL_DBMS_NAME, buf, 256, 0);
	if(SUCCESS(r)) printf(_("DBMS:    %s\n"), buf);

	r = SQLGetInfo(conn, SQL_DBMS_VER, buf, 256, 0);
	if(SUCCESS(r)) printf(_("Version: %s\n"), buf);

	fputs("\n", stdout);

	return conn;
}

results *db_conn_info(SQLHDBC conn)
{
	SQLRETURN r;
	results *res;
	char buf[256];

	res = calloc(1, sizeof(struct results));

	res->ncols = 2;
	res->cols = calloc(2, sizeof(char *));
	res->cols[0] = strdup(_("name"));
	res->cols[1] = strdup(_("value"));

	r = SQLGetInfo(conn, SQL_SERVER_NAME, buf, 256, 0);
	if(SUCCESS(r)) {

	}

	r = SQLGetInfo(conn, SQL_DBMS_NAME, buf, 256, 0);
	if(SUCCESS(r)) printf(_("DBMS:    %s\n"), buf);

	r = SQLGetInfo(conn, SQL_DBMS_VER, buf, 256, 0);
	if(SUCCESS(r)) printf(_("Version: %s\n"), buf);

	return res;
}

results *execute_query(SQLHDBC conn, const char *buf)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SUCCESS(r)) {
		printf(_("Failed to allocate statement handle\n"));
		return 0;
	}

	gettimeofday(&taken, 0);

	current_statement = &st;
	r = SQLExecDirect(st, (SQLCHAR *) buf, SQL_NTS);

	time_taken(&taken);

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to execute statement\n"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return 0;
	}

	return fetch_results(st, taken);
}

static results *fetch_results(SQLHSTMT st, struct timeval time_taken)
{
	// TODO: check return value of mallocs

	results *res;
	SQLRETURN r;
	SQLSMALLINT i;
	SQLINTEGER j;

	res = calloc(1, sizeof(results));

	res->time_taken = time_taken;

	SQLGetDiagField(SQL_HANDLE_STMT, st, 0, SQL_DIAG_NUMBER, &(res->nwarnings), 0, 0);
	if(res->nwarnings) {
		res->warnings = calloc(res->nwarnings, sizeof(char *));
		for(j = 0; j < res->nwarnings; j++) {
			SQLCHAR buf[1024];  // TODO: cope with data bigger than this
			SQLGetDiagField(SQL_HANDLE_STMT, st, j + 1,
					SQL_DIAG_MESSAGE_TEXT, buf, 1024, 0);
			res->warnings[j] = strdup((char *) buf);
		}
	}

	r = SQLRowCount(st, &(res->nrows));
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to retrieve number of rows\n"));
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
			printf(_("Failed to retrieve number of columns\n"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		results_free(res);
		return 0;
	}

	if(!res->ncols) {
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return res;
	}

	res->cols = calloc(res->ncols, sizeof(char *));

	for(i = 0; i < res->ncols; i++) {
		SQLCHAR buf[1024];  // TODO: cope with data bigger than this
		SQLSMALLINT type;
		SQLUINTEGER size;
		SQLSMALLINT digits;
		SQLSMALLINT nullable;

		// TODO: is there a way to just get the name?
		r = SQLDescribeCol(st, i + 1, buf, 256, 0,
				   &type, &size, &digits, &nullable);
		if(!SUCCESS(r)) {
			if(!report_error(SQL_HANDLE_STMT, st))
				printf(_("Failed to retrieve column data\n"));
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


	res->data = calloc(res->nrows, sizeof(char **));
	for(j = 0; j < res->nrows; j++) {
		r = SQLFetch(st);
		if(!SUCCESS(r)) {
			if(!report_error(SQL_HANDLE_STMT, st))
				printf(_("Failed to fetch row %ld (apparently %ld rows)\n"), j, res->nrows);
			SQLFreeHandle(SQL_HANDLE_STMT, st);
			results_free(res);
			return 0;
		}

		res->data[j] = calloc(res->ncols, sizeof(char *));

		for(i = 0; i < res->ncols; i++) {
			char buf[1024];  // TODO: cope with data bigger than this
			SQLINTEGER len;
			r = SQLGetData(st, i + 1, SQL_C_CHAR, buf, 1024, &len);
			if(!SUCCESS(r)) {
				if(!report_error(SQL_HANDLE_STMT, st))
					printf(_("Failed to fetch column %d from row %ld\n"), i, j);
				SQLFreeHandle(SQL_HANDLE_STMT, st);
				results_free(res);
				return 0;
			}

			if(len != SQL_NULL_DATA) res->data[j][i] = strdup(buf);
		}
	}

	current_statement = 0;
	SQLFreeHandle(SQL_HANDLE_STMT, st);
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
			printf(_("Failed to allocate statement handle\n"));
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
			printf(_("Failed to list tables\n"));
	}

	return fetch_results(st, taken);
}

results *get_columns(SQLHDBC conn, const char *catalog,
			const char *schema, const char *table)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval taken;

	gettimeofday(&taken, 0);
	current_statement = st;
	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	time_taken(&taken);

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to allocate statement handle\n"));
		return 0;
	}

	r = SQLColumns(st,
		      (SQLCHAR *) catalog, SQL_NTS,
		      (SQLCHAR *) schema, SQL_NTS,
		      (SQLCHAR *) table, SQL_NTS,
		      (SQLCHAR *) 0, 0);
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to list columns\n"));
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
