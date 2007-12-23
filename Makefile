OBJS = main.o action.o buffer.o command.o db.o err.o history.o output.o prompt.o rc.o results.o signal.o
CFLAGS = -Wall -Werror `guile-config compile`

dbsh: ${OBJS}
	gcc -g -o $@ ${OBJS} -lodbc -lreadline -lefence `guile-config link`

clean:
	rm -f dbsh *.o
