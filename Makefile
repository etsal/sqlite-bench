CFLAGS=-Wall -I$(PWD) -O2 -DNDEBUG -std=c99
SRCS=benchmark.c histogram.c main.c random.c raw.c sqlite3.c util.c
LDFLAGS=-pthread -ldl -lm -static
SQLITE_CFLAGS=-DSQLITE_OMIT_AUTOVACUUM
SQLITE_CFLAGS_MMAP=-DSQLITE_DEFAULT_MMAP_SIZE=536870912 -DSQLITE_MAX_MMAP_SIZE=2147483648 -DSQLITE_MMAP_READWRITE
CC=clang

all: db_bench_mmap db_bench_default

db_bench_mmap: $(SRCS) 
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) $(SQLITE_CFLAGS_MMAP) $(LDFLAGS) $(SRCS) -o $@

db_bench_default: $(SRCS) 
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	rm -rf db_bench_* *.o

distclean: clean
	rm -rf sqlite.*

.PHONY: clean distclean
