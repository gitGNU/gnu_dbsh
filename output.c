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
#include <wchar.h>

#include "common.h"
#include "err.h"
#include "output.h"
#include "results.h"


typedef struct {
	int lines;
	int *widths;
	int max_width;
} dim;

typedef struct {
	dim **col_dims;
	dim **row_dims;
} res_dims;


#define NULL_DISPLAY "*NULL*"


static wchar_t *strdup2wcs(const char *s)
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

static dim *get_dimensions(const wchar_t *s)
{
	dim *d;
	const wchar_t *p;
	int line, w;

	if(!(d = calloc(1, sizeof(dim)))) err_system();

	d->lines = 1;
	for(p = s; *p; p++)  if(*p == L'\n') d->lines++;

	if(!(d->widths = calloc(d->lines, sizeof(int)))) err_system();

	line = 0;
	for(p = s; *p; p++) {
		if(*p == L'\n') {
			if(d->widths[line] > d->max_width)
				d->max_width = d->widths[line];
			line++;
		} else if(*p == L'\t') {
			d->widths[line] += 8;
		} else {
			w = wcwidth(*p);
			if(w > 0) d->widths[line] += w;
		}
	}

	if(d->widths[line] > d->max_width) d->max_width = d->widths[line];

	return d;
}

static void free_dimensions(dim *d)
{
	free(d->widths);
	free(d);
}

static res_dims *get_resultset_dimensions(resultset *res)
{
	res_dims *rd;
	SQLSMALLINT i;
	SQLINTEGER j;
	row *r;
	wchar_t *wcs;

	if(!(rd = malloc(sizeof(res_dims)))) err_system();
	if(!(rd->col_dims = calloc(res->ncols, sizeof(dim)))) err_system();
	if(!(rd->row_dims = calloc(res->ncols * res->nrows, sizeof(dim)))) err_system();

	for(i = 0; i < res->ncols; i++) {
		wcs = strdup2wcs(res->cols[i]);
		rd->col_dims[i] = get_dimensions(wcs);
		free(wcs);

		for(r = res->rows, j = 0; r; r = r->next, j++) {
			wcs = strdup2wcs(r->data[i] ? r->data[i] : NULL_DISPLAY);
			rd->row_dims[(j * res->ncols) + i] = get_dimensions(wcs);
			free(wcs);
		}
	}

	return rd;
}

static void free_resultset_dimensions(resultset *res, res_dims *rd)
{
	SQLSMALLINT i;
	SQLINTEGER j;

	for(i = 0; i < res->ncols; i++) {
		free_dimensions(rd->col_dims[i]);
		for(j = 0; j < res->nrows; j++) {
			free_dimensions(rd->row_dims[(j * res->ncols) + i]);
		}
	}

	free(rd->col_dims);
	free(rd->row_dims);
	free(rd);
}

static void output_size(resultset *res, FILE *s)
{
	fprintf(s, res->nrows == 1 ?
		_("1 row in set\n") :
		_("%ld rows in set\n"),
		res->nrows);

}

void output_horiz_separator(FILE *s, int col_widths[], SQLSMALLINT ncols)
{
	SQLSMALLINT i;
	int j;

	for(i = 0; i < ncols; i++) {
		fputc('+', s);
		for(j = 0; j < col_widths[i] + 2; j++) fputc('-', s);
	}
	fputs("+\n", s);
}

void output_horiz_row(FILE *s, const char **data,
		      dim **dims, int widths[], SQLSMALLINT ncols)
{
	const char **pos, *p;
	SQLSMALLINT i;
	int j, k, more_lines;

	pos = calloc(ncols, sizeof(char *));
	memcpy(pos, data, ncols * sizeof(char *));

	for(i = 0; i < ncols; i++) if(!pos[i]) pos[i] = NULL_DISPLAY;

	j = 0;
	do {
		more_lines = 0;

		for(i = 0; i < ncols; i++) {

			fputs("| ", s);

			for(p = pos[i]; *p; p++) {
				if(*p == '\n') {
					pos[i] = p + 1;
					more_lines = 1;
					break;
				} else if(*p == '\t') {
					fputs("        ", s);
				} else {
					fputc(*p, s);
				}
			}

			if(!*p) pos[i] = p;

			for(k = dims[i]->widths[j]; k <= widths[i]; k++) fputc(' ', s);
		}

		fputs("|\n", s);
		j++;

	} while(more_lines);

	free(pos);
}

void output_horiz(resultset *res, FILE *s)
{
	res_dims *dims;
	SQLSMALLINT i;
	SQLINTEGER j;
	row *r;
	int *col_widths;


	dims = get_resultset_dimensions(res);
	if(!(col_widths = calloc(res->ncols, sizeof(int)))) err_system();

	for(i = 0; i < res->ncols; i++) {
		col_widths[i] = dims->col_dims[i]->max_width;

		for(j = 0; j < res->nrows; j++) {
			if(dims->row_dims[j * res->ncols + i]->max_width > col_widths[i])
				col_widths[i] = dims->row_dims[j * res->ncols + i]->max_width;
		}
	}

	output_horiz_separator(s, col_widths, res->ncols);
	output_horiz_row(s, (const char **) res->cols, dims->col_dims, col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols);
	for(r = res->rows, j = 0; r; r = r->next, j++)
		output_horiz_row(s, (const char **) r->data, dims->row_dims + (j * res->ncols), col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols);

	free_resultset_dimensions(res, dims);

	output_size(res, s);
}

void output_vert_separator(FILE *s, int col_width, int row_width)
{
	int k;

	fputc('+', s);
	for(k = 0; k <= col_width + 1; k++) fputc('-', s);
	fputc('+', s);
	for(k = 0; k <= row_width + 1; k++) fputc('-', s);
	fputs("+\n", s);
}

void output_vert(resultset *res, FILE *s)
{
	res_dims *dims;
	int col_width, row_width;
	SQLSMALLINT i;
	SQLINTEGER j;
	row *r;
	int k, l;
	char *p;


	dims = get_resultset_dimensions(res);

	col_width = 0;
	row_width = 0;
	for(i = 0; i < res->ncols; i++) {
		if(dims->col_dims[i]->max_width > col_width) col_width = dims->col_dims[i]->max_width;

		for(j = 0; j < res->nrows; j++) {
			if(dims->row_dims[j * res->ncols + i]->max_width > row_width)
				row_width = dims->row_dims[j * res->ncols + i]->max_width;
		}
	}

	for(r = res->rows, j = 0; r; r = r->next, j++) {

		output_vert_separator(s, col_width, row_width);

		for(i = 0; i < res->ncols; i++) {

			l = 0;

			fputc('|', s);
			for(k = 0; k <= col_width - dims->col_dims[i]->widths[0]; k++) fputc(' ', s);

			for(p = res->cols[i]; *p; p++) {
				if(*p == '\t') fputs("        ", s);
				else fputc(*p, s);
			}

			fputs(" | ", s);

			if(r->data[i]) {
				for(p = r->data[i]; *p; p++) {
					if(*p == '\n') {
						for(k = 0; k < row_width - dims->row_dims[j * res->ncols + i]->widths[l]; k++) fputc(' ', s);
						fputs(" |\n|", s);
						for(k = 0; k < col_width + 1; k++) fputc(' ', s);
						fputs(" | ", s);
						l++;
					} else if(*p == '\t') {
						fputs("        ", s);
					} else {
						fputc(*p, s);
					}
				}
			} else {
				fputs(NULL_DISPLAY, s);
			}

			for(k = 0; k < row_width - dims->row_dims[j * res->ncols + i]->widths[l]; k++) fputc(' ', s);
			fputs(" |\n", s);
		}
	}

	if(j) output_vert_separator(s, col_width, row_width);


	free_resultset_dimensions(res, dims);

	output_size(res, s);
}

void output_csv_row(FILE *s, const char **data, SQLSMALLINT ncols, char separator, char delimiter)
{
	SQLSMALLINT i;
	const char *p;

	for(i = 0; i < ncols; i++) {

		if(delimiter) fputc(delimiter, s);

		if(data[i]) {
			for(p = data[i]; *p; p++) {
				if(*p == delimiter) fputc(delimiter, s);
				fputc(*p, s);
			}
		}

		if(delimiter) fputc(delimiter, s);

		if(i < ncols - 1) fputc(separator, s);
	}

	fputc('\n', s);
}

void output_csv(resultset *res, FILE *s, char separator, char delimiter)
{
	row *r;

	output_csv_row(s, (const char **) res->cols, res->ncols, separator, delimiter);
	for(r = res->rows; r; r = r->next)
		output_csv_row(s, (const char **) r->data, res->ncols, separator, delimiter);
}

void output_results(results *res, char mode, FILE *s)
{
	SQLINTEGER i;
	resultset *set;

	if(mode == 1) mode = *getenv("DBSH_DEFAULT_ACTION");

	for(set = res->sets; set; set = set->next) {
		if(set->nrows == -1) {
			fputs(_("Success\n"), s);
		} else if(!set->ncols) {
			fprintf(s, _("%ld rows affected\n"), set->nrows);
		} else {
			switch(mode) {
			case 'C':  // CSV
				output_csv(set, s, ',', '"');
				break;
			case 'G':  // Vertical
				output_vert(set, s);
				break;
			case 'H':  // HTML
				fprintf(s, "TODO\n");
				break;
			case 'J':  // JSON
				fprintf(s, "TODO\n");
				break;
			case 'T':  // TSV
				output_csv(set, s, '\t', 0);
				break;
			case 'X':  // XMLS
				fprintf(s, "TODO\n");
				break;
			default:
				output_horiz(set, s);
			}
			fputc('\n', s);
		}
	}

	for(i = 0; i < res->nwarnings; i++) {
		fprintf(s, "WARNING: %s\n", res->warnings[i]);
	}

	if((mode == 'G' || mode == 'g') &&
	   (res->time_taken.tv_sec || res->time_taken.tv_usec))
		fprintf(s, _("(%lu.%06lus)\n"),
			res->time_taken.tv_sec, res->time_taken.tv_usec);
}
