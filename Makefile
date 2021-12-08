.PHONY: all
.DEFAULT_GOAL := all

server:
	gcc -Wall -Werror  -c server.c
	gcc server.o -o server udp.c

client:
	gcc -fPIC -g -c -Wall libmfs.c
	gcc -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o -lc udp.c
	gcc -o main main.c -Wall -L. -lmfs

clean:
	rm -rf libmfs.o libmfs.so main server server.o

all: server client