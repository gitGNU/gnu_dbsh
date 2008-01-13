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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "err.h"
#include "results.h"


static void v_resultset_set_cols(resultset *, int, va_list);
static row *v_resultset_add_row(resultset *, va_list);


wchar_t *strdup2wcs(const char *s)
{
	mbstate_t ps;
	size_t len;
	wchar_t *wcs;

	memset(&ps, 0, sizeof(ps));

	len = mbsrtowcs(0, &s, 0, &ps);
	if(len == -1) err_system();

	if(!(wcs = calloc(len + 1, sizeof(wchar_t)))) err_system();
	mbsrtowcs(wcs, &s, len, &ps);

	return wcs;
}

results *results_alloc()
{
	results *res;

	if(!(res = malloc(sizeof(results)))) err_system();

	res->sets = 0;
	res->nwarnings = 0;
	res->warnings = 0;
	res->time_taken.tv_sec = 0;
	res->time_taken.tv_usec = 0;

	return res;
}

results *results_single_alloc()
{
	results *res = results_alloc();
	res->sets = resultset_alloc();

	return res;
}

resultset *resultset_alloc()
{
	resultset *res;

	if(!(res = malloc(sizeof(resultset)))) err_system();

	res->ncols = 0;
	res->nrows = 0;
	res->cols = 0;
	res->rows = 0;
	res->next = 0;

	return res;
}

row *results_row_alloc(int ncols)
{
	row *row;

	if(!(row = malloc(sizeof(row))) ||
	   !(row->data = calloc(ncols, sizeof(wchar_t *))))
		err_system();

	row->next = 0;

	return row;
}

resultset *results_add_set(results *res)
{
	resultset **sp;

	for(sp = &res->sets; *sp; sp = &(*sp)->next);
	*sp = resultset_alloc();

	return *sp;
}

void results_set_warnings(results *res, int nwarnings, ...)
{
	va_list ap;
	int i;

	va_start(ap, nwarnings);

	res->nwarnings = nwarnings;
	if(!(res->warnings = calloc(nwarnings, sizeof(char *)))) err_system();

	for(i = 0; i < nwarnings; i++)
		if(!(res->warnings[i] = strdup2wcs(va_arg(ap, const char *)))) err_system();

	va_end(ap);
}

void results_set_cols(results *res, int ncols, ...)
{
	va_list ap;

	va_start(ap, ncols);
	v_resultset_set_cols(res->sets, ncols, ap);
	va_end(ap);
}

void resultset_set_cols(resultset *res, int ncols, ...)
{
	va_list ap;

	va_start(ap, ncols);
	v_resultset_set_cols(res, ncols, ap);
	va_end(ap);
}

static void v_resultset_set_cols(resultset *res, int ncols, va_list ap)
{
	int i;

	res->ncols = ncols;
	if(!(res->cols = calloc(ncols, sizeof(char *)))) err_system();

	for(i = 0; i < ncols; i++)
		if(!(res->cols[i] = strdup2wcs(va_arg(ap, const char *)))) err_system();
}

row *results_add_row(results *res, ...)
{
	va_list ap;
	row *r;

	va_start(ap, res);
	r = v_resultset_add_row(res->sets, ap);
	va_end(ap);

	return r;
}

row *resultset_add_row(resultset *res, ...)
{
	va_list ap;
	row *r;

	va_start(ap, res);
	r = v_resultset_add_row(res, ap);
	va_end(ap);

	return r;
}

static row *v_resultset_add_row(resultset *res, va_list ap)
{
	row **rp;
	int i;
	const char *d;

	/*
	  This is inefficient but I think it makes the code
	  clearer than passing a 'latest row' around.
	*/

	for(rp = &(res->rows); *rp; rp = &(*rp)->next);
	*rp = results_row_alloc(res->ncols);

	for(i = 0; i < res->ncols; i++) {
		d = va_arg(ap, const char *);
		if(d && !((*rp)->data[i] = strdup2wcs(d))) err_system();
	}

	res->nrows++;

	return *rp;
}

void results_free(results *r)
{
	SQLINTEGER i;

	if(r->warnings) {
		for(i = 0; i < r->nwarnings; i++) if(r->warnings[i]) free(r->warnings[i]);
		free(r->warnings);
	}

	if(r->sets) resultset_free(r->sets);

	free(r);
}

void resultset_free(resultset *r)
{
	SQLSMALLINT i;

	if(r->next) resultset_free(r->next);

	if(r->cols) {
		for(i = 0; i< r->ncols; i++) if(r->cols[i]) free(r->cols[i]);
		free(r->cols);
	}

	if(r->rows) results_row_free(r->rows, r->ncols);

	free(r);
}

void results_row_free(row *r, int ncols)
{
	SQLSMALLINT i;

	if(r->next) results_row_free(r->next, ncols);

	if(r->data) {
		for(i = 0; i < ncols; i++) if(r->data[i]) free(r->data[i]);
		free(r->data);
	}

	free(r);
}
