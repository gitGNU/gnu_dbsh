#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "action.h"
#include "buffer.h"
#include "command.h"
#include "output.h"


static void go(SQLHDBC conn, sql_buffer *sqlbuf, char action)
{
	db_results *res;

	if(sqlbuf->buf[0] == '*') {  // TODO: configurable command character
		res = run_command(conn, sqlbuf->buf);
	} else {
		res = execute_query(conn, sqlbuf->buf);
	}
	if(res) {
		output_results(res, action, stdout);
		free_results(res);
	}
}

static void edit(sql_buffer *sqlbuf)
{
	char *editor;
	char path[1024];
	char cmd[2014];
	int f;

	editor = getenv("EDITOR");
	if(!editor) editor = getenv("VISUAL");
	if(!editor) editor = "vi";

	// FIXME: make this safe
	snprintf(path, 1024, "/tmp/dbsh-edit.%d", getpid());

	f = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if(f == -1) {
		perror("Failed to open temporary file");
		return;
	}
	write(f, sqlbuf->buf, sqlbuf->next - 2);
	close(f);

	snprintf(cmd, 1024, "%s %s", editor, path);
	system(cmd);

	f = open(path, O_RDONLY);
	if(f != -1) {
		sqlbuf->next = read(f, sqlbuf->buf, sqlbuf->len - 1);
		close(f);
		sqlbuf->buf[sqlbuf->next++] = 0;
	}

	unlink(path);
}

static void print(sql_buffer *sqlbuf)
{
	printf("%s\n", sqlbuf->buf);
	sqlbuf->next--;
}

int run_action(SQLHDBC conn, sql_buffer *sqlbuf, char action)
{
	int reset = 0;

	switch(action) {
	case 'C':  // CSV
	case 'g':  // horizontal
	case 'G':  // vertical
	case 'h':  // HTML
	case 'j':  // JSON
	case 't':  // TSV
	case 'x':  // XML
		go(conn, sqlbuf, action);
		reset = 1;
		break;
	case 'c':
		sqlbuf->next = 0;
		reset = 1;
		break;
	case 'e':
		edit(sqlbuf);
		print(sqlbuf);
		break;
	case 'l':
		// TODO: load named buffer (or should that be a command?)
		break;
	case 'p':
		print(sqlbuf);
		break;
	case 's':
		// TODO: save to named buffer
		break;
	}

	return reset;
}
