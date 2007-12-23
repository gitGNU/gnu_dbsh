/*
 TODO: unicode support
       filter out dodgy chars
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "output.h"
#include "results.h"


#define NULL_DISPLAY "*NULL*"
#define NULL_WIDTH 6


void output_warnings(results *res, FILE *s)
{
	SQLINTEGER i;
	for(i = 0; i < res->nwarnings; i++) {
		fprintf(s, "%s\n", res->warnings[i]);
	}
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

void output_horiz_row(FILE *s, const char **data, int widths[], SQLSMALLINT ncols)
{
	SQLSMALLINT i;
	int j, len;

	for(i = 0; i < ncols; i++) {
		fputs("| ", s);

		if(data[i]) {
			len = strlen(data[i]);
			if(len > widths[i]) len = widths[i];
			fwrite(data[i], 1, len, s);
		} else {
			len = NULL_WIDTH;
			fputs(NULL_DISPLAY, s);
		}

		if(len < widths[i]) {
			for(j = 0; j < widths[i] - len; j++) {
				fputc(' ', s);
			}
		}

		fputc(' ', s);
	}

	fputs("|\n", s);
}

void output_horiz(results *res, FILE *s)
{
	SQLSMALLINT i;
	SQLINTEGER j;
	int *col_widths;

 	col_widths = calloc(res->ncols, sizeof(int));

	for(i = 0; i < res->ncols; i++) {
		col_widths[i] = strlen(res->cols[i]);
	}

	for(j = 0; j < res->nrows; j++) {
		for(i = 0; i < res->ncols; i++) {
			int w;
			if(res->data[j][i]) w = strlen(res->data[j][i]);
			else w = NULL_WIDTH;
			if(w > col_widths[i]) col_widths[i] = w;
		}
	}

	output_horiz_separator(s, col_widths, res->ncols);
	output_horiz_row(s, (const char **) res->cols, col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols);
	for(j = 0; j < res->nrows; j++)
		output_horiz_row(s, (const char **) res->data[j], col_widths, res->ncols);
	output_horiz_separator(s, col_widths, res->ncols);

	free(col_widths);

	output_warnings(res, s);
	fprintf(s, _("%ld rows in set (%ld.%06lds)\n"), res->nrows,
		res->time_taken.tv_sec, res->time_taken.tv_usec);
}

void output_vert(results *res, FILE *s)
{
	int col_width;
	SQLSMALLINT i;
	SQLINTEGER j;
	int k;

	col_width = 0;
	for(i = 0; i < res->ncols; i++) {
		int w;
		w = strlen(res->cols[i]);
		if(w > col_width) col_width = w;
	}

	col_width++;

	for(j = 0; j < res->nrows; j++) {

		fprintf(s, "******************************** Row %ld ********************************\n", j + 1);

		for(i = 0; i < res->ncols; i++) {

			for(k = 0; k < col_width - strlen(res->cols[i]); k++) {
				fputc(' ', s);
			}

			fprintf(s, "%s | %s\n", res->cols[i], res->data[j][i] ? res->data[j][i] : NULL_DISPLAY);
		}
	}

	output_warnings(res, s);
	fprintf(s, _("%ld rows in set (%ld.%06lds)\n"), res->nrows,
		res->time_taken.tv_sec, res->time_taken.tv_usec);
}

void output_csv_row(FILE *s, const char **data, SQLSMALLINT ncols, char separator, char delimiter)
{
	SQLSMALLINT i;
	int j;

	for(i = 0; i < ncols; i++) {

		if(delimiter) fputc(delimiter, s);

		if(data[i]) {
			for(j = 0; j < strlen(data[i]); j++) {
				if(data[i][j] == delimiter) fputc(delimiter, s);
				fputc(data[i][j], s);
			}
		}

		if(delimiter) fputc(delimiter, s);

		if(i < ncols - 1) fputc(separator, s);
	}

	fputc('\n', s);
}

void output_csv(results *res, FILE *s, char separator, char delimiter)
{
	SQLINTEGER j;

	output_csv_row(s, (const char **) res->cols, res->ncols, separator, delimiter);
	for(j = 0; j < res->nrows; j++)
		output_csv_row(s, (const char **) res->data[j], res->ncols, separator, delimiter);
}

void output_results(results *res, char mode, FILE *s)
{
	if(res->nrows == -1) {
		output_warnings(res, s);
		fputs(_("Success\n"), s);
	} else if(!res->ncols) {
		output_warnings(res, s);
		fprintf(s, _("%ld rows affected (%ld.%06lds)\n"), res->nrows,
			res->time_taken.tv_sec, res->time_taken.tv_usec);
	} else {

		switch(mode) {
		case 'C':  // CSV
			output_csv(res, s, ',', '"');
			break;
		case 'G':  // Vertical
			output_vert(res, s);
			break;
		case 'H':  // HTML
			fprintf(s, "TODO\n");
			break;
		case 'J':  // JSON
			fprintf(s, "TODO\n");
			break;
		case 'T':  // TSV
			output_csv(res, s, '\t', 0);
			break;
		case 'X':  // XMLS
			fprintf(s, "TODO\n");
			break;
		default:
			output_horiz(res, s);
		}
	}
}
