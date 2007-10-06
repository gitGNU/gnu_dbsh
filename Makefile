OBJS = main.o action.o buffer.o command.o db.o output.o
CFLAGS = -Wall

dbsh: ${OBJS}
	gcc -g -o $@ ${OBJS} -lodbc -lreadline -lefence

clean:
	rm dbsh *.o
