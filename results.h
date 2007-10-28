#ifndef RESULTS_H
#define RESULTS_H

#include <sys/time.h>
#include <time.h>

typedef struct db_results db_results;

struct db_results {
	SQLSMALLINT ncols;
	SQLINTEGER nrows;
	SQLINTEGER nwarnings;
	char **cols;
	char ***data;
	char **warnings;
	struct timeval time_taken;
};

db_results *alloc_results();
void free_results(db_results *);

#endif
