#include "common.h"
#include "action.h"
#include "command.h"
#include "output.h"

int run_action(SQLHDBC conn, sql_buffer *sqlbuf, char action)
{
	int reset = 0;
	db_results *res;

	switch(action) {
	case 'c':  // CSV
	case 'g':  // horizontal
	case 'G':  // vertical
	case 'h':  // HTML
	case 'j':  // JSON
	case 't':  // TSV
	case 'x':  // XML
		if(sqlbuf->buf[0] == '*') {  // TODO: configurable command character
			res = run_command(conn, sqlbuf->buf);
		} else {
			res = execute_query(conn, sqlbuf->buf);
		}
		if(res) {
			output_results(res, action, stdout);
			free_results(res);
		}
		reset = 1;
		break;
	case 'e':
		// TODO: edit
		break;
	case 'l':
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':
		printf("%s\n", sqlbuf->buf);
		sqlbuf->next--;
		break;
	case 's':
		// TODO: save to named buffer
		break;
	}

	return reset;
}
