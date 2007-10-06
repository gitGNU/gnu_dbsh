OBJS = main.o action.o buffer.o command.o db.o output.o
CFLAGS = -Wall -Werror

dbsh: ${OBJS}
	gcc -g -o $@ ${OBJS} -lodbc -lreadline -lefence

clean:
	rm dbsh *.o
