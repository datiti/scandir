#
# 
# Required Ubuntu packages: libulfius-dev libmicrohttpd-dev
#
CC=gcc
CFLAGS=-D_REENTRANT
# -pthread == -lpthread -D_REENTRANT
# -lulfius for internal rest web server
# -ljansson to manipulate json structure
LDFLAGS=-pthread -lulfius -ljansson
EXEC=main

all: $(EXEC)

$(EXEC): main.o
	$(CC) -o $(EXEC) main.o $(LDFLAGS)

main.o: main.c main.h
	$(CC) -o main.o -c main.c $(CFLAGS)

clean: 
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)

