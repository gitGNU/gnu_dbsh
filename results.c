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

/*
  This whole implementation is horribly inefficient.
  But that's fixable.  At least it has some semblance of structure now.
  Premature optimisation etc etc.
*/


#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "err.h"
#include "results.h"


typedef struct warn warn;
typedef struct set set;
typedef struct row row;

struct results {
	set *sets;
	set *scursor;
	row *rcursor;
	warn *warnings;
	warn *wcursor;
	struct timeval time_taken;
};

struct warn {
	wchar_t *text;
	warn *next;
};

struct set {
	unsigned int ncols;
	unsigned int nrows;
	wchar_t **cols;
	row *rows;
	set *next;
};

struct row {
	wchar_t **data;
	row *next;
};


static wchar_t *strdup2wcs(const char *);
static wchar_t *wstrdup(const wchar_t *);

static void warn_free(warn *);

static set *set_alloc();
static void set_free(set *);
static set *current_set(results *);

static row *row_alloc();
static void row_free(row *, unsigned int);
static row *current_row(results *);


results *res_alloc()
{
	results *res;

	if(!(res = malloc(sizeof(results)))) err_system();

	res->sets = set_alloc();
	res->scursor = res->sets;
	res->rcursor = 0;
	res->warnings = 0;
	res->wcursor = 0;
	res->time_taken.tv_sec = 0;
	res->time_taken.tv_usec = 0;

	return res;
}

void res_free(results *r)
{
	if(r->warnings) warn_free(r->warnings);
	if(r->sets) set_free(r->sets);
	free(r);
}

void res_start_timer(results *r)
{
	gettimeofday(&r->time_taken, 0);
}

void res_stop_timer(results *r)
{
	struct timeval end_time;

	gettimeofday(&end_time, 0);

	r->time_taken.tv_sec  = end_time.tv_sec  - r->time_taken.tv_sec;
	r->time_taken.tv_usec = end_time.tv_usec - r->time_taken.tv_usec;
}

struct timeval res_time_taken(results *r)
{
	return r->time_taken;
}

void res_add_warning(results *r, const char *text)

{
	warn **wp;

	for(wp = &r->warnings; *wp; wp = &(*wp)->next);

	if(!(*wp = malloc(sizeof(warn)))) err_system();
	(*wp)->text = strdup2wcs(text);
	(*wp)->next = 0;

	if(!r->wcursor) r->wcursor = *wp;
}

wchar_t *res_next_warning(results *r)
{
	wchar_t *text;

	if(!r->wcursor) return 0;
	text = r->wcursor->text;
	r->wcursor = r->wcursor->next;
	return text;
}

void res_add_set(results *r)
{
	set **sp;

	for(sp = &r->sets; *sp; sp = &(*sp)->next);
	*sp = set_alloc();
	r->scursor = *sp;
}

void res_first_set(results *r)
{
	r->scursor = r->sets;
	r->rcursor = 0;
}

int res_next_set(results *r)
{
	if(r->scursor) r->scursor = r->scursor->next;
	r->rcursor = 0;
	return r->scursor ? 1 : 0;
}

void res_set_ncols(results *r, unsigned int ncols)
{
	set *s;

	s = current_set(r);
	if(s->nrows) err_fatal("res_set_ncols: meta set");
	s->ncols = ncols;
	if(!(s->cols = realloc(s->cols, ncols * sizeof(wchar_t *)))) err_system();
}

void res_set_col(results *r, unsigned int i, const char *text)
{
	set *s;

	s = current_set(r);
	if(i >= s->ncols) err_fatal("res_set_col: %u, '%s' (%u columns)",
				    i, text, s->ncols);
	s->cols[i] = strdup2wcs(text);
}

void res_set_cols(results *r, unsigned int ncols, ...)
{
	va_list ap;
	unsigned int i;

	res_set_ncols(r, ncols);
	va_start(ap, ncols);
	for(i = 0; i < ncols; i++) res_set_col(r, i, va_arg(ap, const char *));
	va_end(ap);
}

unsigned int res_get_ncols(results *r)
{
	set *s;

	s = current_set(r);
	return s->ncols;
}

wchar_t *res_get_col(results *r, unsigned int i)
{
	set *s;

	s = current_set(r);
	if(i >= s->ncols) err_fatal("res_get_col: %u (%u columns)",
				    i, s->ncols);
	return s->cols[i];
}

wchar_t **res_get_cols(results *r)
{
	set *s;

	s = current_set(r);
	return s->cols;
}

void res_set_nrows(results *r, int nrows)
{
	set *s;

	s = current_set(r);
	if(s->ncols) err_fatal("res_set_nrows: data set");
	s->nrows = nrows;
}

void res_new_row(results *res)
{
	set *s;
	row **rp;

	s = current_set(res);
	for(rp = &s->rows; *rp; rp = &(*rp)->next);
	*rp = row_alloc(s->ncols);

	res->rcursor = *rp;
	s->nrows++;
}

void res_set_value(results *res, unsigned int i, const char *value)
{
	set *s;
	row *r;

	s = current_set(res);
	if(i >= s->ncols) err_fatal("res_set_value: %u, '%s' (%u columns)",
				    i, value, s->ncols);

	r = current_row(res);
	if(r->data[i]) free(r->data[i]);
	r->data[i] = strdup2wcs(value);
}

void res_set_value_w(results *res, unsigned int i, const wchar_t *value)
{
	set *s;
	row *r;

	s = current_set(res);
	if(i >= s->ncols) err_fatal("res_set_value_w: %u, '%ls' (%u columns)",
				    i, value, s->ncols);

	r = current_row(res);
	if(r->data[i]) free(r->data[i]);
	r->data[i] = wstrdup(value);
}

void res_add_row(results *res, ...)
{
	set *s;
	va_list ap;
	unsigned int i;

	s = current_set(res);
	res_new_row(res);
	va_start(ap, res);
	for(i = 0; i < s->ncols; i++)
		res_set_value(res, i, va_arg(ap, const char *));
	va_end(ap);
}

int res_get_nrows(results *res)
{
	set *s;

	s = current_set(res);
	return s->nrows;
}

int res_next_row(results *res)
{
	set *s;

	if(res->rcursor) {
		res->rcursor = res->rcursor->next;
	} else {
		s = current_set(res);
		res->rcursor = s->rows;
	}

	return res->rcursor ? 1 : 0;
}

int res_more_rows(results *res)
{
	row *r;

	r = current_row(res);
	return r->next ? 1 : 0;
}

wchar_t *res_get_value(results *res, unsigned int i)
{
	return current_row(res)->data[i];
}

wchar_t **res_get_row(results *res)
{
	return current_row(res)->data;
}



static wchar_t *strdup2wcs(const char * s)
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

static wchar_t *wstrdup(const wchar_t *s)
{
	wchar_t *d;
	size_t l;

	l = (wcslen(s) + 1) * sizeof(wchar_t);
	if(!(d = malloc(l))) err_system();
	memcpy(d, s, l);

	return d;
}

static void warn_free(warn *w)
{
	if(w->next) warn_free(w->next);
	if(w->text) free(w->text);
	free(w);
}

static set *set_alloc()
{
	set *res;

	if(!(res = malloc(sizeof(set)))) err_system();

	res->ncols = 0;
	res->nrows = 0;
	res->cols = 0;
	res->rows = 0;
	res->next = 0;

	return res;
}

static void set_free(set *r)
{
	int i;

	if(r->next) set_free(r->next);

	if(r->cols) {
		for(i = 0; i< r->ncols; i++) if(r->cols[i]) free(r->cols[i]);
		free(r->cols);
	}

	if(r->rows) row_free(r->rows, r->ncols);

	free(r);
}

static set *current_set(results *r)
{
	if(!r->scursor) err_fatal("current_set: no current set");
	return r->scursor;
}

static row *row_alloc(int ncols)
{
	row *r;

	if(!(r = malloc(sizeof(row))) ||
	   !(r->data = calloc(ncols, sizeof(wchar_t *))))
		err_system();

	r->next = 0;

	return r;
}

static void row_free(row *r, unsigned int ncols)
{
	int i;

	if(r->next) row_free(r->next, ncols);

	if(r->data) {
		for(i = 0; i < ncols; i++) if(r->data[i]) free(r->data[i]);
		free(r->data);
	}

	free(r);
}

static row *current_row(results *res)
{
	if(!res->rcursor) err_fatal("current_row: no current row");
	return res->rcursor;
}
