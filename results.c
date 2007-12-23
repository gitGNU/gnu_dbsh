#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "err.h"
#include "results.h"


results *results_alloc()
{
	results *res;

	if(!(res = calloc(1, sizeof(struct results)))) err_system();
	return res;
}

void results_set_cols(results *res, int ncols, ...)
{
	va_list ap;
	int i;

	va_start(ap, ncols);

	res->ncols = ncols;
	if(!(res->cols = calloc(ncols, sizeof(char *)))) err_system();

	for(i = 0; i < ncols; i++)
		if(!(res->cols[i] = strdup(va_arg(ap, const char *)))) err_system();

	va_end(ap);
}

void results_set_warnings(results *res, int nwarnings, ...)
{
	va_list ap;
	int i;

	va_start(ap, nwarnings);

	res->nwarnings = nwarnings;
	if(!(res->cols = calloc(nwarnings, sizeof(char *)))) err_system();

	for(i = 0; i < nwarnings; i++)
		if(!(res->warnings[i] = strdup(va_arg(ap, const char *)))) err_system();

	va_end(ap);
}

void results_set_rows(results *res, int nrows)
{
	int i;

	res->nrows = nrows;
	if(!(res->data = calloc(nrows, sizeof(char **)))) err_system();

	for(i = 0; i < nrows; i++)
		if(!(res->data[i] = calloc(res->ncols, sizeof(char *)))) err_system();
}

void results_free(results *r)
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

	if(r->warnings) {
		for(j = 0; j < r->nwarnings; j++) if(r->warnings[j]) free(r->warnings[j]);
		free(r->warnings);
	}

	free(r);
}

