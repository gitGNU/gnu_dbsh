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

#ifndef RESULTS_H
#define RESULTS_H

#include <sys/time.h>
#include <sql.h>
#include <wchar.h>

typedef struct resultset resultset;
typedef struct row row;

struct results {
	resultset *sets;
	SQLINTEGER nwarnings;
	wchar_t **warnings;
	struct timeval time_taken;
};

struct resultset {
	SQLSMALLINT ncols;
	SQLINTEGER nrows;
	wchar_t **cols;
	row *rows;
	resultset *next;
};

struct row {
	wchar_t **data;
	row *next;
};

results *results_alloc();
results *results_single_alloc();
resultset *results_add_set();
void results_set_warnings(results *, int, ...);
void results_set_cols(results *, int, ...);
row *results_add_row(results *, ...);
void results_free(results *);

resultset *resultset_alloc();
void resultset_set_cols(resultset *, int, ...);
row *resultset_add_row(resultset *, ...);
void resultset_free(resultset *);

row *results_row_alloc(int);
void results_row_free(row *, int);

wchar_t *strdup2wcs(const char *);

#endif
