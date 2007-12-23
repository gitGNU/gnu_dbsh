#ifndef RESULTS_H
#define RESULTS_H

#include <sql.h>

typedef struct results results;

struct results {
	SQLSMALLINT ncols;
	SQLINTEGER nrows;
	SQLINTEGER nwarnings;
	char **cols;
	char ***data;
	char **warnings;
	struct timeval time_taken;
};

results *results_alloc();
void results_set_cols(results *, int, ...);
void results_set_warnings(results *, int, ...);
void results_set_rows(results *, int);
void results_free(results *);

#endif
