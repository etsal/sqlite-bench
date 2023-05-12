SQLITEDIR=$(PWD)/../sqlite
CFLAGS=-Wall -O2 -DNDEBUG -std=c99 
SRCS=benchmark.c histogram.c main.c random.c raw.c util.c
LDFLAGS=-pthread -ldl -lm -lsqlite3 
CC=clang

all: db_bench_mmap db_bench_default

test_default: db_bench_default
	echo `sysctl hw.model`
	uname -a | cut -f 3,4,5
	time ./db_bench_default --num=4000

db_bench_mmap: $(SRCS) 
	$(CC) -I$(SQLITEDIR)/build_mmap/ -L$(SQLITEDIR)/build_mmap/ $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

db_bench_default: $(SRCS) 
	$(CC) -I$(SQLITEDIR)/build_default/ -L$(SQLITEDIR)/build_default/ $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	rm -rf db_bench_* *.o

.PHONY: all test clean
