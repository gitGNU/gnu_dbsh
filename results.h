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
#include <wchar.h>


results *res_alloc();
void res_free(results *);

void res_start_timer(results *);
void res_stop_timer(results *);
struct timeval res_time_taken(results *);

void res_add_warning(results *, const char *);
wchar_t *res_next_warning(results *);

void res_add_set(results *);
void res_first_set(results *);
int res_next_set(results *);

void res_set_ncols(results *, unsigned int);
void res_set_col(results *, unsigned int, const char *);
void res_set_cols(results *, unsigned int, ...);
unsigned int res_get_ncols(results *);
wchar_t *res_get_col(results *, unsigned int);
wchar_t **res_get_cols(results *);

void res_set_nrows(results *, int);
void res_new_row(results *);
void res_set_value(results *, unsigned int, const char *);
void res_set_value_w(results *, unsigned int, const wchar_t *);
void res_add_row(results *, ...);
int res_get_nrows(results *);
int res_next_row(results *);
int res_more_rows(results *);
wchar_t *res_get_value(results *, unsigned int);
wchar_t **res_get_row(results *);

#endif
