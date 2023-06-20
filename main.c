// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"

// Comma-separated list of operations to run in the specified order
char* FLAGS_benchmarks;

// Number of key/values to place in the database.
int FLAGS_num_keys;

// Number of operations to do for the benchmark.
long FLAGS_num_ops;

// Number of read operations to do.  If negative, do FLAGS_num reads.
int FLAGS_reads;

// Size of each value
int FLAGS_value_size;

// Print histogram of operation timings
bool FLAGS_histogram;

// Print raw data
bool FLAGS_raw;

// Arrange to generate values that shrink to this fraction of
// their original size after compression
double FLAGS_compression_ratio;

// Page size. Default 1 KB.
int FLAGS_page_size;

// Number of pages.
// Default cache size = FLAGS_page_size * FLAGS_num_pages = 4 MB.
int FLAGS_num_pages;

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
bool FLAGS_use_existing_db;

// If true then single operations, not transactions.
bool FLAGS_benchmark_single_op;

// If true, we allow batch writes to occur
bool FLAGS_transaction;

// If true, we enable Write-Ahead Logging
bool FLAGS_WAL_enabled;

// Configure how many pages to use for WAL
int FLAGS_checkpoint_granularity;

// Configure the write percentage for mixed read/write benchmarks.
int FLAGS_write_percent;

// Configure the maximum mmap size in MB.
int FLAGS_mmap_size_mb;

// Configure the batch size for the transactional benchmarks.
int FLAGS_batch_size;

// Configure the default SLS OID (or 0 if no SLS).
int FLAGS_oid;

// Use the db with the following name.
char* FLAGS_db;

// Load the following extension.
char* FLAGS_extension;

void init() {
  // Comma-separated list of operations to run in the specified order
  //   Actual benchmarks:
  //
  //   fillseq       -- write N values in sequential key order in async mode
  //   fillseqsync   -- write N/100 values in sequential key order in sync mode
  //   fillseqbatch  -- batch write N values in sequential key order in async mode
  //   fillrandom    -- write N values in random key order in async mode
  //   fillrandsync  -- write N/100 values in random key order in sync mode
  //   fillrandbatch -- batch write N values in sequential key order in async mode
  //   readseq       -- read N times sequentially
  //   readrandom    -- read N times in random order
  //   rwrandom   	-- write N values in random key order in async mode
  //   rwrandsync 	-- write N/100 values in random key order in sync mode
  //   rwrandbatch	-- batch write N values in sequential key order in async mode
  //   rwseq   		-- write N values in random key order in async mode
  //   rwseqsync 	-- write N/100 values in random key order in sync mode
  //   rwseqbatch	-- batch write N values in sequential key order in async mode
  FLAGS_benchmarks =
    "fillseq,"
    "fillseqsync,"
    "fillseqbatch,"
    "fillrandom,"
    "fillrandsync,"
    "fillrandbatch,"
    "readrandom,"
    "readseq,"
    "rwrandom,"
    "rwrandsync,"
    "rwseq,"
    "rwseqsync,"
    ;
  FLAGS_reads = -1;
  FLAGS_value_size = 100;
  FLAGS_histogram = false;
  FLAGS_raw = false,
  FLAGS_compression_ratio = 0.5;
  FLAGS_page_size = 4096;
  FLAGS_num_pages = 4096;
  FLAGS_use_existing_db = false;
  FLAGS_transaction = true;
  FLAGS_benchmark_single_op = false,
  FLAGS_WAL_enabled = true;
  FLAGS_checkpoint_granularity = 1024;
  FLAGS_write_percent = 50;
  FLAGS_mmap_size_mb = 4;
  FLAGS_oid = 0;
  FLAGS_db = NULL;
  FLAGS_batch_size = 1024;
  FLAGS_extension = NULL;
  FLAGS_num_keys = 50000;
  FLAGS_num_ops = 50000;
}

void print_usage(const char* argv0) {
  fprintf(stderr, "Usage: %s [OPTION]...\n", argv0);
  fprintf(stderr, "SQLite3 benchmark tool\n");
  fprintf(stderr, "[OPTION]\n");
  fprintf(stderr, "  --benchmarks=[BENCH]\t\tspecify benchmark\n");
  fprintf(stderr, "  --histogram={0,1}\t\trecord histogram\n");
  fprintf(stderr, "  --raw={0,1}\t\t\toutput raw data\n");
  fprintf(stderr, "  --compression_ratio=DOUBLE\tcompression ratio\n");
  fprintf(stderr, "  --use_existing_db={0,1}\tuse existing database\n");
  fprintf(stderr, "  --num_keys=INT\t\t\tnumber of keys\n");
  fprintf(stderr, "  --num_ops=INT\t\t\tnumber of operations\n");
  fprintf(stderr, "  --reads=INT\t\t\tnumber of reads\n");
  fprintf(stderr, "  --value_size=INT\t\tvalue size\n");
  fprintf(stderr, "  --no_transaction\t\tdisable transaction\n");
  fprintf(stderr, "  --benchmark_single_op\t\tstatistics for individual operations, not transactions\n");
  fprintf(stderr, "  --page_size=INT\t\tpage size\n");
  fprintf(stderr, "  --num_pages=INT\t\tnumber of pages\n");
  fprintf(stderr, "  --WAL_enabled={0,1}\t\tenable WAL\n");
  fprintf(stderr, "  --WAL_size=INT\t\tWAL size in pages\n");
  fprintf(stderr, "  --write_percent=INT\t\twrite %% in rw benchmarks\n");
  fprintf(stderr, "  --mmap_size_mb=INT\t\tMBs of memory region size for mmap IO\n");
  fprintf(stderr, "  --db=PATH\t\t\tpath to location databases are created\n");
  fprintf(stderr, "  --extension=NAME\t\tname of extension to be loaded\n");
  fprintf(stderr, "  --help\t\t\tshow this help\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "[BENCH]\n");
  fprintf(stderr, "  fillseq\twrite N values in sequential key order in async mode\n");
  fprintf(stderr, "  fillseqsync\twrite N/100 values in sequential key order in sync mode\n");
  fprintf(stderr, "  fillseqbatch\tbatch write N values in sequential key order in async mode\n");
  fprintf(stderr, "  fillrandom\twrite N values in random key order in async mode\n");
  fprintf(stderr, "  fillrandsync\twrite N/100 values in random key order in sync mode\n");
  fprintf(stderr, "  fillrandbatch\tbatch write N values in random key order in async mode\n");
  fprintf(stderr, "  overwrite\toverwrite N values in random key order in async mode\n");
  fprintf(stderr, "  fillrand100K\twrite N/1000 100K values in random order in async mode\n");
  fprintf(stderr, "  fillseq100K\twirte N/1000 100K values in sequential order in async mode\n");
  fprintf(stderr, "  readseq\tread N times sequentially\n");
  fprintf(stderr, "  readrandom\tread N times in random order\n");
  fprintf(stderr, "  readrand100K\tread N/1000 100K values in sequential order in async mode\n");

}

int main(int argc, char** argv) {
  init();

  char* default_db_path = malloc(sizeof(char) * 1024);
  strcpy(default_db_path, "./");

  for (int i = 1; i < argc; i++) {
    double d;
    long l;
    int n;
    char junk;
    if (starts_with(argv[i], "--benchmarks=")) {
      FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_histogram = n;
    } else if (sscanf(argv[i], "--raw=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_raw = n;
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      FLAGS_compression_ratio = d;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_use_existing_db = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      FLAGS_reads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      FLAGS_value_size = n;
    } else if (!strcmp(argv[i], "--no_transaction")) {
      FLAGS_transaction = false;
    } else if (!strcmp(argv[i], "--benchmark_single_op")) {
      FLAGS_benchmark_single_op = true;
    } else if (sscanf(argv[i], "--page_size=%d%c", &n, &junk) == 1) {
      FLAGS_page_size = n;
    } else if (sscanf(argv[i], "--num_pages=%d%c", &n, &junk) == 1) {
      FLAGS_num_pages = n;
    } else if (sscanf(argv[i], "--num_ops=%ld%c", &l, &junk) == 1) {
      FLAGS_num_ops = l;
    } else if (sscanf(argv[i], "--num_keys=%d%c", &n, &junk) == 1) {
      FLAGS_num_keys = n;
    } else if (sscanf(argv[i], "--WAL_enabled=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_WAL_enabled = n;
    } else if (sscanf(argv[i], "--checkpoint_granularity=%d%c", &n, &junk) == 1) {
      FLAGS_checkpoint_granularity = n;
    } else if (sscanf(argv[i], "--write_percent=%d%c", &n, &junk) == 1) {
      FLAGS_write_percent = n;
    } else if (sscanf(argv[i], "--mmap_size_mb=%d%c", &n, &junk) == 1) {
      FLAGS_mmap_size_mb = n;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      FLAGS_db = argv[i] + 5;
    } else if (sscanf(argv[i], "--oid=%d%c", &n, &junk) == 1) {
      FLAGS_oid = n;
    } else if (sscanf(argv[i], "--batch_size=%d%c", &n, &junk) == 1) {
      FLAGS_batch_size = n;
    } else if (strncmp(argv[i], "--extension=", 12) == 0) {
      FLAGS_extension = argv[i] + 12;
    } else if (!strcmp(argv[i], "--help")) {
      print_usage(argv[0]);
      exit(0);
    } else {
      fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
      exit(1);
    }
  }

  /* Choose a location for the test database if none given with --db=<path>  */
  if (FLAGS_db == NULL)
      FLAGS_db = default_db_path;

  benchmark_init();
  benchmark_run();
  benchmark_fini();

  return 0;}
