SQLITEDIR=$(PWD)/../sqlite
CFLAGS=-Wall -O2 -DNDEBUG -std=c99 -g
SRCS=benchmark.c histogram.c main.c random.c raw.c util.c $(SQLITEDIR)/build/sqlite3.c
INCLUDEDIR=-I$(SQLITEDIR)/build
LDFLAGS=-pthread -ldl -lm -lsls
CC=clang

all: db_bench

test: db_bench
	echo `sysctl hw.model`
	uname -a | cut -f 3,4,5
	time ./db_bench --num=4000
	rm -f *.db *.db-wal

db_bench: $(SRCS) 
	$(CC) $(INCLUDEDIR) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	rm -rf db_bench* *.o *.db *.db-wal

.PHONY: all test clean
