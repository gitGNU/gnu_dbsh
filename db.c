#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"


int list_dsns(SQLHENV, SQLUSMALLINT);

#define SUCCESS(r) (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO)
#define report_error(t, h) _report_error(t, h,  __FILE__, __LINE__)


int _report_error(SQLSMALLINT type, SQLHANDLE handle, const char *file, int line)
{
	SQLRETURN r;
	SQLCHAR message[256];
	SQLCHAR state[6];
	SQLINTEGER code;

	r = SQLGetDiagRec(type, handle, 1, state, &code, message, 256, 0);

	if(SUCCESS(r)) {
		// TODO: only include file and line in debug mode
		printf(_("%s (SQLSTATE %s, error %ld) (%s:%d)\n"), message, state, code, file, line);
		return 1;
	} else {
		return 0;
	}
}

SQLHENV alloc_env()
{
	SQLHENV env;
	SQLRETURN r;


	r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	if(!SUCCESS(r)) {
		printf(_("Failed to allocate environment handle\n"));
		exit(1);
	}

	r = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
	if(!SUCCESS(r)) {
		printf(_("Failed to set ODBC version to 3\n"));
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		exit(1);
	}

	return env;
}

void list_all_dsns(SQLHENV env)
{
	printf(_("User Data Sources\n-----------------\n"));
	if(!list_dsns(env, SQL_FETCH_FIRST_USER)) printf(_("(none)\n"));
	printf(_("\nSystem Data Sources\n-------------------\n"));
	if(!list_dsns(env, SQL_FETCH_FIRST_SYSTEM)) printf(_("(none)\n"));
}

int list_dsns(SQLHENV env, SQLUSMALLINT dir)
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
		printf(_("Failed to allocate connection handle\n"));
		SQLFreeHandle(SQL_HANDLE_ENV, env);
		exit(1);
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

	r = SQLGetInfo(conn, SQL_DBMS_NAME, buf, 256, 0);
	if(SUCCESS(r)) printf(_("DBMS:    %s\n"), buf);

	r = SQLGetInfo(conn, SQL_DBMS_VER, buf, 256, 0);
	if(SUCCESS(r)) printf(_("Version: %s\n"), buf);

	fputs("\n", stdout);

	return conn;
}

db_results *execute_query(SQLHDBC conn, const char *buf)
{
	SQLHSTMT st;
	SQLRETURN r;
	struct timeval tstart, tfinish;

	db_results *res;
	SQLSMALLINT i;
	SQLINTEGER j;


	// TODO: check return value of mallocs

	r = SQLAllocHandle(SQL_HANDLE_STMT, conn, &st);
	if(!SUCCESS(r)) {
		printf(_("Failed to allocate statement handle\n"));
		return 0;
	}

	gettimeofday(&tstart, 0);
	r = SQLExecDirect(st, (SQLCHAR *) buf, SQL_NTS);
	gettimeofday(&tfinish, 0);

	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to execute statement\n"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		return 0;
	}

	res = calloc(1, sizeof(db_results));

	res->time_taken.tv_sec  = tfinish.tv_sec  - tstart.tv_sec;
	res->time_taken.tv_usec = tfinish.tv_usec - tstart.tv_usec;

	r = SQLRowCount(st, &(res->nrows));
	if(!SUCCESS(r)) {
		if(!report_error(SQL_HANDLE_STMT, st))
			printf(_("Failed to retrieve number of rows\n"));
		SQLFreeHandle(SQL_HANDLE_STMT, st);
		free_results(res);
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
		free_results(res);
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
			free_results(res);
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
				printf(_("Failed to fetch row %ld\n"), j);
			SQLFreeHandle(SQL_HANDLE_STMT, st);
			free_results(res);
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
				free_results(res);
				return 0;
			}

			if(len != SQL_NULL_DATA) res->data[j][i] = strdup(buf);
		}
	}

	SQLFreeHandle(SQL_HANDLE_STMT, st);
	return res;
}

void free_results(db_results *r)
{
	SQLSMALLINT i;
	SQLINTEGER j;

	if(r->cols) {
		for(i = 0; i< r->ncols; i++) if(r->cols[i]) free(r->cols[i]);
		free(r->cols);
	}
	if(r->data) {
		for(j = 0; j < r->nrows; j++) {
			if(r->data[j]) {
				for(i = 0; i < r->ncols; i++) {
					if(r->data[j][i]) free(r->data[j][i]);
				}
				free(r->data[j]);
			}
		}
		free(r->data);
	}

	free(r);
}
