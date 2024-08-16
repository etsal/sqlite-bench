SQLITEDIR=$(PWD)/../sqlite
CFLAGS=-Wall -O2 -DNDEBUG -std=c99 -g
SRCS=benchmark.c histogram.c main.c random.c raw.c util.c $(SQLITEDIR)/build/sqlite3.c
SQLITE_FLAGS=-DSQLITE_DQS=0 \
	-DSQLITE_THREADSAFE=0 \
	-DSQLITE_DEFAULT_MEMSTATUS=0 \
	-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 \
	-DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
	-DSQLITE_MAX_EXPR_DEPTH=0 \
	-DSQLITE_OMIT_DECLTYPE \
	-DSQLITE_OMIT_DEPRECATED \
	-DSQLITE_OMIT_PROGRESS_CALLBACK \
	-DSQLITE_OMIT_SHARED_CACHE \
	-DSQLITE_USE_ALLOCA \
	-DSQLITE_MMAP_READWRITE \
	-DSQLITE_MAX_MMAP_SIZE=1073741824
INCLUDEDIR=-I$(SQLITEDIR)/build
LDFLAGS=-pthread -ldl -lm 
CC=clang

all: db_bench db_bench_objsnap

test: db_bench
	echo `sysctl hw.model`
	uname -a | cut -f 3,4,5
	time ./db_bench --num=4000
	rm -f *.db *.db-wal

db_bench: $(SRCS) 
	$(CC) $(INCLUDEDIR) $(CFLAGS) $(SQLITE_FLAGS) $(LDFLAGS) $(SRCS) -lsls -o $@

db_bench_objsnap: $(SRCS) 
	$(CC) $(INCLUDEDIR) $(CFLAGS) $(SQLITE_FLAGS) $(LDFLAGS) $(SRCS) -DUSE_MSNP_OBJSNP -lmsnp -o $@

clean:
	rm -rf db_bench* *.o *.db *.db-wal

.PHONY: all test clean
