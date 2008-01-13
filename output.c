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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "common.h"
#include "err.h"
#include "output.h"
#include "results.h"
#include "stream.h"


typedef struct {
	int lines;
	int *widths;
	int max_width;
} dim;

typedef struct {
	dim **col_dims;
	dim **row_dims;
} res_dims;

typedef enum {
	VPOS_TOP,
	VPOS_MID,
	VPOS_BOT
} vpos;

typedef enum {
	HPOS_LEF,
	HPOS_MID,
	HPOS_RIG
} hpos;

#define NULL_DISPLAY L"*NULL*"



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

	if(!(rd = malloc(sizeof(res_dims)))) err_system();
	if(!(rd->col_dims = calloc(res->ncols, sizeof(dim)))) err_system();
	if(!(rd->row_dims = calloc(res->ncols * res->nrows, sizeof(dim)))) err_system();

	for(i = 0; i < res->ncols; i++) {
		rd->col_dims[i] = get_dimensions(res->cols[i]);
		for(r = res->rows, j = 0; r; r = r->next, j++) {
			rd->row_dims[(j * res->ncols) + i] =
				get_dimensions(r->data[i] ? r->data[i] : NULL_DISPLAY);
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

static const char *get_box_char(vpos v, hpos h)
{
	switch(v) {
	case VPOS_TOP:
		switch(h) {
		case HPOS_LEF:
			return pgettext("topleft", "+");
		case HPOS_MID:
			return pgettext("topmid", "+");
		case HPOS_RIG:
			return pgettext("topright", "+");
		}
	case VPOS_MID:
		switch(h) {
		case HPOS_LEF:
			return pgettext("midleft", "+");
		case HPOS_MID:
			return pgettext("midmid", "+");
		case HPOS_RIG:
			return pgettext("midright", "+");
		}
		break;
	case VPOS_BOT:
		switch(h) {
		case HPOS_LEF:
			return pgettext("botleft", "+");
		case HPOS_MID:
			return pgettext("botmid", "+");
		case HPOS_RIG:
			return pgettext("botright", "+");
		}
		break;
	}
}

static void output_size(resultset *res, stream *s)
{
	stream_printf(s,
		      ngettext("1 row in set", "%ld rows in set", res->nrows),
		      res->nrows);
}

void output_horiz_separator(stream *s, int col_widths[], SQLSMALLINT ncols, vpos v)
{
	hpos h;
	SQLSMALLINT i;
	int j;

	h = HPOS_LEF;

	for(i = 0; i < ncols; i++) {
		stream_puts(s, get_box_char(v, h));
		for(j = 0; j < col_widths[i] + 2; j++) stream_puts(s,  _("-"));
		h = HPOS_MID;
	}
	stream_puts(s, get_box_char(v, HPOS_RIG));
	stream_newline(s);
}

void output_horiz_row(stream *s, const char **data,
		      dim **dims, int widths[], SQLSMALLINT ncols)
{
	const wchar_t **pos, *p;
	SQLSMALLINT i;
	int j, k, more_lines;

	pos = calloc(ncols, sizeof(wchar_t *));
	memcpy(pos, data, ncols * sizeof(wchar_t *));

	for(i = 0; i < ncols; i++) if(!pos[i]) pos[i] = NULL_DISPLAY;

	j = 0;
	do {
		more_lines = 0;

		for(i = 0; i < ncols; i++) {

			stream_puts(s, _("|"));
			stream_space(s);

			for(p = pos[i]; *p; p++) {
				if(*p == L'\n') {
					pos[i] = p + 1;
					more_lines = 1;
					break;
				} else if(*p == L'\t') {
					stream_putws(s, L"        ");
				} else {
					stream_putwc(s, *p);
				}
			}

			if(!*p) pos[i] = p;

			for(k = dims[i]->widths[j]; k <= widths[i]; k++) stream_space(s);
		}

		stream_puts(s, _("|"));
		stream_newline(s);
		j++;

	} while(more_lines);

	free(pos);
}

void output_horiz(resultset *res, stream *s)
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

	output_horiz_separator(s, col_widths, res->ncols, VPOS_TOP);
	output_horiz_row(s, (const char **) res->cols, dims->col_dims, col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols, VPOS_MID);
	for(r = res->rows, j = 0; r; r = r->next, j++)
		output_horiz_row(s, (const char **) r->data, dims->row_dims + (j * res->ncols), col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols, VPOS_BOT);

	free_resultset_dimensions(res, dims);

	output_size(res, s);
}

void output_vert_separator(stream *s, int col_width, int row_width, vpos v)
{
	int k;

	stream_puts(s, get_box_char(v, HPOS_LEF));
	for(k = 0; k <= col_width + 1; k++) stream_puts(s, _("-"));
	stream_puts(s, get_box_char(v, HPOS_MID));
	for(k = 0; k <= row_width + 1; k++) stream_puts(s, _("-"));
	stream_puts(s, get_box_char(v, HPOS_RIG));
	stream_newline(s);
}

void output_vert(resultset *res, stream *s)
{
	res_dims *dims;
	int col_width, row_width;
	SQLSMALLINT i;
	SQLINTEGER j;
	vpos v;
	row *r;
	int k, l;
	wchar_t *p;


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

	v = VPOS_TOP;

	for(r = res->rows, j = 0; r; r = r->next, j++) {

		output_vert_separator(s, col_width, row_width, v);

		for(i = 0; i < res->ncols; i++) {

			l = 0;

			stream_puts(s, _("|"));
			for(k = 0; k <= col_width - dims->col_dims[i]->widths[0]; k++) stream_space(s);

			for(p = res->cols[i]; *p; p++) {
				if(*p == '\t') stream_putws(s, L"        ");
				else stream_putwc(s, *p);
			}

			stream_space(s);
			stream_puts(s, _("|"));
			stream_space(s);

			if(r->data[i]) {
				for(p = r->data[i]; *p; p++) {
					if(*p == L'\n') {
						for(k = 0; k < row_width - dims->row_dims[j * res->ncols + i]->widths[l] + 1; k++) stream_space(s);
						stream_puts(s, _("|"));
						stream_newline(s);
						stream_puts(s, _("|"));
						for(k = 0; k < col_width + 2; k++) stream_space(s);
						stream_puts(s, _("|"));
						stream_space(s);
						l++;
					} else if(*p == L'\t') {
						stream_putws(s, L"        ");
					} else {
						stream_putwc(s, *p);
					}
				}
			} else {
				stream_putws(s, NULL_DISPLAY);
			}

			for(k = 0; k < row_width - dims->row_dims[j * res->ncols + i]->widths[l] + 1; k++) stream_space(s);
			stream_puts(s, _("|"));
			stream_newline(s);
		}

		v = VPOS_MID;
	}

	if(j) output_vert_separator(s, col_width, row_width, VPOS_BOT);

	free_resultset_dimensions(res, dims);

	output_size(res, s);
}

void output_csv_row(stream *s, wchar_t **data, SQLSMALLINT ncols, wchar_t sep, wchar_t delim)
{
	SQLSMALLINT i;
	const wchar_t *p;

	for(i = 0; i < ncols; i++) {

		if(delim) stream_putwc(s, delim);

		if(data[i]) {
			for(p = data[i]; *p; p++) {
				if(*p == delim) stream_putwc(s, delim);
				stream_putwc(s, *p);
			}
		}

		if(delim) stream_putwc(s, delim);

		if(i < ncols - 1) stream_putwc(s, sep);
	}

	stream_newline(s);
}

void output_csv(resultset *res, stream *s, char separator, char delimiter)
{
	row *r;

	output_csv_row(s, res->cols, res->ncols, separator, delimiter);
	for(r = res->rows; r; r = r->next)
		output_csv_row(s, r->data, res->ncols, separator, delimiter);
}

void output_results(results *res, char mode, stream *s)
{
	SQLINTEGER i;
	resultset *set;

	if(mode == 1) mode = *getenv("DBSH_DEFAULT_ACTION");

	for(set = res->sets; set; set = set->next) {
		if(set->nrows == -1) {
			stream_puts(s, _("Success\n"));
		} else if(!set->ncols) {
			stream_printf(s, _("%ld rows affected\n"), set->nrows);
		} else {
			switch(mode) {
			case 'C':  // CSV
				output_csv(set, s, L',', L'"');
				break;
			case 'G':  // Vertical
				output_vert(set, s);
				break;
			case 'H':  // HTML
				stream_printf(s, "TODO\n");
				break;
			case 'J':  // JSON
				stream_printf(s, "TODO\n");
				break;
			case 'T':  // TSV
				output_csv(set, s, L'\t', 0);
				break;
			case 'X':  // XMLS
				stream_printf(s, "TODO\n");
				break;
			default:
				output_horiz(set, s);
			}
			stream_newline(s);
		}
	}

	for(i = 0; i < res->nwarnings; i++) {
		stream_putws(s, res->warnings[i]);
		stream_newline(s);
	}

	if((mode == 'G' || mode == 'g') &&
	   (res->time_taken.tv_sec || res->time_taken.tv_usec))
		stream_printf(s, _("(%lu.%06lus)\n"),
			res->time_taken.tv_sec, res->time_taken.tv_usec);
}
