CC = gcc

telnetchat: main.o
	${CC} ${LDFLAGS} main.o -o telnetchat
	chmod +x telnetchat

main.o: main.c
	${CC} ${CFLAGS} -c main.c
