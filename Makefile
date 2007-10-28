OBJS = main.o action.o buffer.o command.o db.o history.o output.o rc.o results.o rsignal.o
CFLAGS = -Wall -Werror `guile-config compile`

dbsh: ${OBJS}
	gcc -g -o $@ ${OBJS} -lodbc -lreadline -lefence `guile-config link`

clean:
	rm dbsh *.o
