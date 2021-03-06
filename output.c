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

#include "cntrl.h"
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



static wchar_t *translate(wchar_t *src)
{
	wchar_t *dest, *p, *q, *r;
	int l;

	if(!src) {
		l = wcslen(cntrl[0]);
		if(!(dest = calloc(l + 1, sizeof(wchar_t)))) err_system();
		for(r = cntrl[0], q = dest; *r; r++) *q++ = *r;

		return dest;
	}

	l = 0;
	for(p = src; *p; p++) {
		if(*p < 32) l += wcslen(cntrl[*p]);
	        else if(*p == 127) l += wcslen(cntrl[32]);
		else l++;
	}

	if(!(dest = calloc(l + 1, sizeof(wchar_t)))) err_system();

	for(p = src, q = dest; *p; p++) {
		if(*p != L'\r' || p[1] != L'\n') {
			if(*p < 32 || *p == 127) {
				for(r = (*p == 127) ? cntrl[32] : cntrl[*p];
				    *r; r++)
					*q++ = *r;
			} else *q++ = *p;
		}
	}

	return dest;
}

static void translate_resultset(results *res)
{
	int i;
	wchar_t *t;

	while(res_next_row(res)) {
		for(i = 0; i < res_get_ncols(res); i++) {
			t = translate(res_get_value(res, i));
			res_set_value_w(res, i, t);
			free(t);
		}
	};
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

static res_dims *get_resultset_dimensions(results *res)
{
	res_dims *rd;
	int ncols, i, j;

	ncols = res_get_ncols(res);

	if(!(rd = malloc(sizeof(res_dims)))) err_system();
	if(!(rd->col_dims = calloc(ncols, sizeof(dim)))) err_system();
	if(!(rd->row_dims = calloc(ncols * res_get_nrows(res), sizeof(dim)))) err_system();

	for(i = 0; i < res_get_ncols(res); i++) {
		rd->col_dims[i] = get_dimensions(res_get_col(res, i));

		j = 0;
		while(res_next_row(res)) {
			rd->row_dims[(j * ncols) + i] =
				get_dimensions(res_get_value(res, i));
			j++;
		};
	}

	return rd;
}

static void free_resultset_dimensions(results *res, res_dims *rd)
{
	int ncols, i, j;

	ncols = res_get_ncols(res);

	for(i = 0; i < ncols; i++) {
		free_dimensions(rd->col_dims[i]);
		for(j = 0; j < res_get_nrows(res); j++) {
			free_dimensions(rd->row_dims[(j * ncols) + i]);
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

	return "+";
}

static void output_size(results *res, stream *s)
{
	int nrows;

	nrows = res_get_nrows(res);
	stream_printf(s,
		      ngettext("1 row in set\n", "%d rows in set\n", nrows),
		      nrows);
}

void output_horiz_separator(stream *s, int col_widths[], int ncols, vpos v)
{
	hpos h;
	int i, j;

	h = HPOS_LEF;

	for(i = 0; i < ncols; i++) {
		stream_puts(s, get_box_char(v, h));
		for(j = 0; j < col_widths[i] + 2; j++) stream_puts(s,  _("-"));
		h = HPOS_MID;
	}
	stream_puts(s, get_box_char(v, HPOS_RIG));
	stream_newline(s);
}

void output_horiz_row(stream *s, wchar_t **data,
		      dim **dims, int widths[], int ncols)
{
	const wchar_t **pos, *p;
	int i, j, k, more_lines;

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

			for(k = j < dims[i]->lines ? dims[i]->widths[j] : 0; k <= widths[i]; k++) stream_space(s);
		}

		stream_puts(s, _("|"));
		stream_newline(s);
		j++;

	} while(more_lines);

	free(pos);
}

void output_horiz(results *res, stream *s)
{
	res_dims *dims;
	int ncols, i, j;
	int *col_widths;

	ncols = res_get_ncols(res);

	translate_resultset(res);
	dims = get_resultset_dimensions(res);

	if(!(col_widths = calloc(ncols, sizeof(int)))) err_system();
	for(i = 0; i < ncols; i++) {

		col_widths[i] = dims->col_dims[i]->max_width;

		for(j = 0; j < res_get_nrows(res); j++) {
			if(dims->row_dims[j * ncols + i]->max_width > col_widths[i])
				col_widths[i] = dims->row_dims[j * ncols + i]->max_width;
		}
	}

	output_horiz_separator(s, col_widths, ncols, VPOS_TOP);
	output_horiz_row(s, res_get_cols(res), dims->col_dims, col_widths, ncols);
	output_horiz_separator(s, col_widths, ncols, VPOS_MID);

	j = 0;
	while(res_next_row(res)) {
		output_horiz_row(s, res_get_row(res),
				 dims->row_dims + (j * ncols),
				 col_widths, ncols);
		j++;
	};

	output_horiz_separator(s, col_widths, ncols, VPOS_BOT);

	free(col_widths);
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

void output_vert(results *res, stream *s)
{
	res_dims *dims;
	int col_width, row_width, ncols, i, j, k, l;
	vpos v;
	wchar_t *p;


	ncols = res_get_ncols(res);

	translate_resultset(res);
	dims = get_resultset_dimensions(res);

	col_width = 0;
	row_width = 0;
	for(i = 0; i < ncols; i++) {
		if(dims->col_dims[i]->max_width > col_width) col_width = dims->col_dims[i]->max_width;

		for(j = 0; j < res_get_nrows(res); j++) {
			if(dims->row_dims[j * ncols + i]->max_width > row_width)
				row_width = dims->row_dims[j * ncols + i]->max_width;
		}
	}

	v = VPOS_TOP;

	j = 0;
	while(res_next_row(res)) {
		output_vert_separator(s, col_width, row_width, v);

		for(i = 0; i < ncols; i++) {

			l = 0;

			stream_puts(s, _("|"));
			for(k = 0; k <= col_width - dims->col_dims[i]->widths[0]; k++) stream_space(s);

			for(p = res_get_col(res, i); *p; p++) {
				if(*p == '\t') stream_putws(s, L"        ");
				else stream_putwc(s, *p);
			}

			stream_space(s);
			stream_puts(s, _("|"));
			stream_space(s);

			for(p = res_get_value(res, i); *p; p++) {
				if(*p == L'\n') {
					for(k = 0; k < row_width - dims->row_dims[j * ncols + i]->widths[l] + 1; k++) stream_space(s);
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

			for(k = 0; k < row_width - dims->row_dims[j * ncols + i]->widths[l] + 1; k++) stream_space(s);
			stream_puts(s, _("|"));
			stream_newline(s);
		}

		v = VPOS_MID;

		j++;
	};

	if(j) output_vert_separator(s, col_width, row_width, VPOS_BOT);

	free_resultset_dimensions(res, dims);

	output_size(res, s);
}

void output_csv_row(stream *s, wchar_t **data, int ncols, wchar_t sep, wchar_t delim)
{
	int i;
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

void output_csv(results *res, stream *s, char separator, char delimiter)
{
	output_csv_row(s, res_get_cols(res), res_get_ncols(res), separator, delimiter);
	while(res_next_row(res)) {
		output_csv_row(s, res_get_row(res), res_get_ncols(res), separator, delimiter);
	};
}

void output_flat(results *res, stream *s)
{
	int i;

	while(res_next_row(res)) {
		for(i = 0; i < res_get_ncols(res); i++) {
			if(res_get_value(res, i)) {
				stream_putws(s, res_get_col(res, i));
				stream_newline(s);
				stream_putws(s, res_get_value(res, i));
				stream_newline(s);
				stream_newline(s);
			}
		}
	};
}

void output_list(results *res, stream *s)
{
	int i;

	for(i = 0; i < res_get_ncols(res); i++) {
		stream_putws(s, res_get_col(res, i));
		stream_putws(s, L": ");

		while(res_next_row(res)) {
			if(res_get_value(res, i)) {
				stream_putws(s, res_get_value(res, i));
				if(res_more_rows(res)) stream_putwc(s, L',');
			}
		};

		stream_newline(s);
	}
}

void output_results(results *res, char mode, stream *s)
{
	wchar_t *w;
	int nrows;
	struct timeval time_taken;

	if(mode == 1) mode = *getenv("DBSH_DEFAULT_ACTION");

	while((w = res_next_warning(res))) {
		stream_putws(s, w);
		stream_newline(s);
	}

	res_first_set(res);

	do {
		nrows = res_get_nrows(res);

		if(nrows == -1) {
			stream_puts(s, _("Success\n"));
		} else if(!res_get_ncols(res)) {
			stream_printf(s,
				      ngettext("1 row affected\n",
					       "%d rows affected\n",
					       nrows),
				      nrows);
		} else {
			switch(mode) {
			case 'C':  // CSV
				output_csv(res, s, L',', L'"');
				break;
			case 'F':  // Flat
				output_flat(res, s);
				break;
			case 'G':  // Vertical
				output_vert(res, s);
				break;
			case 'H':  // HTML
				stream_printf(s, "TODO\n");
				break;
			case 'J':  // JSON
				stream_printf(s, "TODO\n");
				break;
			case 'L':  // List
				output_list(res, s);
				break;
			case 'T':  // TSV
				output_csv(res, s, L'\t', 0);
				break;
			case 'X':  // XMLS
				stream_printf(s, "TODO\n");
				break;
			default:
				output_horiz(res, s);
			}
			stream_newline(s);
		}
	} while(res_next_set(res));

	time_taken = res_time_taken(res);

	if((mode == 'G' || mode == 'g') &&
	   (time_taken.tv_sec || time_taken.tv_usec))
		stream_printf(s, _("(%lu.%06lus)\n"),
			      time_taken.tv_sec, time_taken.tv_usec);
}
