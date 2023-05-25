SQLITEDIR=$(PWD)/../sqlite
CFLAGS=-Wall -O2 -DNDEBUG -std=c99 
SRCS=benchmark.c histogram.c main.c random.c raw.c util.c $(SQLITEDIR)/build/sqlite3.c
INCLUDEDIR=-I$(SQLITEDIR)/build
LDFLAGS=-pthread -ldl -lm
CC=clang

all: db_bench

test: db_bench
	echo `sysctl hw.model`
	uname -a | cut -f 3,4,5
	time ./db_bench --num=4000

db_bench: $(SRCS) 
	$(CC) $(INCLUDEDIR) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	rm -rf db_bench* *.o

.PHONY: all test clean
