OBJS = main.o action.o buffer.o command.o db.o output.o
CFLAGS = -Wall -Werror `guile-config compile`

dbsh: ${OBJS}
	gcc -g -o $@ ${OBJS} -lodbc -lreadline -lefence `guile-config link`

clean:
	rm dbsh *.o
