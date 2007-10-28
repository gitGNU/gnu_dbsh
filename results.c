#include "results.h"

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

	if(r->warnings) {
		for(j = 0; j < r->nwarnings; j++) if(r->warnings[j]) free(r->warnings[j]);
		free(r->warnings);
	}

	free(r);
}
