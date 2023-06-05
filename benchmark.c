// Copyright (c 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "bench.h"

enum Order {
  SEQUENTIAL,
  RANDOM
};

enum DBState {
  FRESH,
  EXISTING
};

sqlite3* db_;
int db_num_;
int num_;
int reads_;
double start_;
double last_op_finish_;
int64_t bytes_;
char* message_;
Histogram hist_;
Raw raw_;
RandomGenerator gen_;
Random rand_;
int done_;

inline
static void exec_error_check(int status, char *err_msg) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    exit(1);
  }
}

inline
static void step_error_check(int status) {
  if (status != SQLITE_DONE) {
    fprintf(stderr, "SQL step error: status = %d\n", status);
    exit(1);
  }
}

inline
static void error_check(int status) {
  if (status != SQLITE_OK) {
    fprintf(stderr, "sqlite3 error: status = %d\n", status);
    exit(1);
  }
}

inline
static void wal_checkpoint(sqlite3* db_) {
  /* Flush all writes to disk */
  if (FLAGS_WAL_enabled)
    sqlite3_wal_checkpoint_v2(db_, NULL, SQLITE_CHECKPOINT_FULL, NULL, NULL);
}

static void print_warnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
  fprintf(stderr,
      "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
      );
#endif
#ifndef NDEBUG
  fprintf(stderr,
      "WARNING: Assertions are enabled: benchmarks unnecessarily slow\n"
      );
#endif
}

static void print_environment() {
  fprintf(stderr, "SQLite:     version %s\n", SQLITE_VERSION);
#if defined(__linux)
  time_t now = time(NULL);
  fprintf(stderr, "Date:       %s", ctime(&now));

  FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo != NULL) {
    char line[1000];
    int num_cpus = 0;
    char* cpu_type = malloc(sizeof(char) * 1000);
    char* cache_size = malloc(sizeof(char) * 1000);
    while (fgets(line, sizeof(line), cpuinfo) != NULL) {
      char* sep = strchr(line, ':');
      if (sep == NULL) {
        continue;
      }
      char* key = calloc(sizeof(char), 1000);
      char* val = calloc(sizeof(char), 1000);
      strncpy(key, line, sep - 1 - line);
      strcpy(val, sep + 1);
      char* trimed_key = trim_space(key);
      char* trimed_val = trim_space(val);
      free(key);
      free(val);
      if (!strcmp(trimed_key, "model name")) {
        ++num_cpus;
        strcpy(cpu_type, trimed_val);
      } else if (!strcmp(trimed_key, "cache size")) {
        strcpy(cache_size, trimed_val);
      }
      free(trimed_key);
      free(trimed_val);
    }
    fclose(cpuinfo);
    fprintf(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type);
    fprintf(stderr, "CPUCache:   %s\n", cache_size);
    free(cpu_type);
    free(cache_size);
  }
#endif
}

static void print_header() {
  const int kKeySize = 16;
  print_environment();
  fprintf(stderr, "Keys:       %d bytes each\n", kKeySize);
  fprintf(stderr, "Values:     %d bytes each\n", FLAGS_value_size);  
  fprintf(stderr, "Entries:    %d\n", num_);
  fprintf(stderr, "RawSize:    %.1f MB (estimated)\n",
            (((int64_t)(kKeySize + FLAGS_value_size) * num_)
            / 1048576.0));
  print_warnings();
  fprintf(stderr, "------------------------------------------------\n");
}

static void start() {
  start_ =  now_micros() * 1e-6;
  bytes_ = 0;
  message_ = malloc(sizeof(char) * 10000);
  strcpy(message_, "");
  last_op_finish_ = start_;
  histogram_clear(&hist_);
  raw_clear(&raw_);
  done_ = 0;
}

void finished_single_op() {
  if (FLAGS_histogram || FLAGS_raw) {
    double now = now_micros() * 1e-6;
    double micros = (now - last_op_finish_) * 1e6;
    if (FLAGS_histogram) {
      histogram_add(&hist_, micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
    }
    if (FLAGS_raw) {
      raw_add(&raw_, micros);
    }
    last_op_finish_ = now;
  }

  done_++;
}

static void stop(const char* name) {
  double finish = now_micros() * 1e-6;

  if (done_ < 1) done_ = 1;

  if (bytes_ > 0) {
    char *rate = malloc(sizeof(char) * 100);;
    snprintf(rate, strlen(rate), "%6.1f MB/s",
              (bytes_ / 1048576.0) / (finish - start_));
    if (message_ && !strcmp(message_, "")) {
      message_ = strcat(strcat(rate, " "), message_);
    } else {
      message_ = rate;
    }
  }

  fprintf(stderr, "%-12s : %11.3f micros/op;%s%s\n",
          name,
          (finish - start_) * 1e6 / done_,
          (!message_ || !strcmp(message_, "") ? "" : " "),
          (!message_) ? "" : message_);
  if (FLAGS_raw) {
    raw_print(stdout, &raw_);
  }
  if (FLAGS_histogram) {
    fprintf(stderr, "Microseconds per op:\n%s\n",
            histogram_to_string(&hist_));
  }
  fflush(stdout);
  fflush(stderr);
}

enum stmt_types {
	STMT_TSTART,
	STMT_TEND,
	STMT_READ,
	STMT_REPLACE,
  STMT_TYPES,
};

sqlite3_stmt *stmts[STMT_TYPES];

char *stmt_text[STMT_TYPES] = {
   "BEGIN TRANSACTION",
   "END TRANSACTION",
   "SELECT * FROM test WHERE key = ?",
   "REPLACE INTO test (key, value) VALUES (?, ?)",
};

void stmt_prepare(void) {
  int status, i;
  for (i = 0; i < STMT_TYPES; i++) {
    status = sqlite3_prepare_v2(db_, stmt_text[i], -1,
                                &stmts[i], NULL);
    error_check(status);
  }
}

void stmt_finalize(void) {
  int status;
  int i;

  for (i = 0; i < STMT_TYPES; i++) {
    status = sqlite3_finalize(stmts[i]);
    error_check(status);
  }
}

#define STMT_SIZE (1024)

static void set_pragma_str(char *pragma, char *val) {
  char stmt[STMT_SIZE];
  char *err_msg;
  int status;
  
  snprintf(stmt, STMT_SIZE, "PRAGMA %s = %s", pragma, val);
  status = sqlite3_exec(db_, stmt, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);
}

static void set_pragma_int(char *pragma, int val) {
  char stmt[STMT_SIZE];
  char *err_msg;
  int status;
  
  snprintf(stmt, STMT_SIZE, "PRAGMA %s = %d", pragma, val);
  status = sqlite3_exec(db_, stmt, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);
}

static void stmt_runonce(sqlite3_stmt *stmt) {
  int status;

  status = sqlite3_step(stmt);
  step_error_check(status);
  status = sqlite3_reset(stmt);
  error_check(status);
}

static void stmt_clear_and_reset(sqlite3_stmt *stmt) {
  int status;

  /* Reset SQLite statement for another use */
  status = sqlite3_clear_bindings(stmt);
  error_check(status);
  status = sqlite3_reset(stmt);
  error_check(status);
}

void setup_sls(int oid) {
  uint64_t epoch;
  int error;

  struct sls_attr attr = {
    .attr_target = SLS_OSD,
    .attr_mode = SLS_DELTA,
    .attr_period = 0,
    .attr_flags = SLSATTR_IGNUNLINKED,
    .attr_amplification = 1,
  };

  error = sls_partadd(oid, attr, -1);
  if (error != 0) {
	  fprintf(stderr, "sls_partadd: error %d\n", error);
	  exit(1);
  }

  error = sls_attach(oid, getpid());
  if (error != 0) {
	  fprintf(stderr, "sls_attach: error %d\n", error);
	  exit(1);
  }

  error = sls_checkpoint_epoch(oid, true, &epoch);
  if (error != 0) {
	  fprintf(stderr, "sls_checkpoint: error %d\n", error);
	  exit(1);
  }

  error = sls_epochwait(oid, epoch, true, NULL);
  if (error != 0) {
	  fprintf(stderr, "sls_epochwait: error %d\n", error);
	  exit(1);
  }
}

static void load_extension(void) {
  sqlite3 *tmpdb;
  char *err_msg;
  int status;

  /* Create a database connection just to import the auroravfs extension. */
  status = sqlite3_open(":memory:", &tmpdb);
  if (status != 0) {
    fprintf(stderr, "open err: %d", status);
    exit(1);
  }

  status = sqlite3_enable_load_extension(tmpdb, 1);
  if (status) {
    fprintf(stderr, "enable extension error: %s\n", sqlite3_errmsg(tmpdb));
    exit(1);
  }

  status = sqlite3_load_extension(tmpdb, FLAGS_extension, NULL, &err_msg);
  if (status) {
    fprintf(stderr, "enable extension error: %s\n", err_msg);
    exit(1);
  }
  
  sqlite3_close(tmpdb);
}

static void benchmark_open_regular(void) {
  char *tmp_dir = "/tmp/";
  char file_name[100];
  int status;

  snprintf(file_name, sizeof(file_name),
		  "%sdbbench_sqlite3-%d.db",
		  tmp_dir,
 		  db_num_);

  status = sqlite3_open(file_name, &db_);
  if (status) {
    fprintf(stderr, "open error: %s\n", sqlite3_errmsg(db_));
    exit(1);
  }
}

static void benchmark_open_slos(void) {
  char *tmp_dir = "/tmp/";
  char file_name[100];
  void *addr;
  int status;

  setup_sls(FLAGS_oid);

  addr = mmap(NULL, FLAGS_mmap_size_mb * 1024 * 1024, PROT_READ | PROT_WRITE, 
		  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (addr == NULL) {
	perror("mmap");
	exit(1);
  }

  /* 
   * Trigger a page fault to force the creation 
   * of the object backing the mapping.
   */
  *(char *)addr= '1';

  if (FLAGS_extension != NULL)
    load_extension();

  snprintf(file_name, sizeof(file_name),
		  "file:%sdbbench_sqlite3-%d.db?ptr=%p&sz=%d&max=%ld&oid=%d&threshold=%d",
		  tmp_dir,
 		  db_num_,
		  addr,
		  0,
		  (long) FLAGS_mmap_size_mb * 1024 * 1024,
		  FLAGS_oid,
		  FLAGS_checkpoint_granularity * 4096);

  status = sqlite3_open_v2(file_name, &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI, FLAGS_extension);
  if (status) {
    fprintf(stderr, "open error: %s\n", sqlite3_errmsg(db_));
    exit(1);
  }

}


static void benchmark_open() {
  char* err_msg = NULL;
  int status;

  assert(db_ == NULL);

  db_num_++;

  /* Open the database. */
  if (FLAGS_oid > 0)
    benchmark_open_slos();
  else
    benchmark_open_regular();

  /* Set the size of the mmap region. */
  set_pragma_int("mmap_size", FLAGS_mmap_size_mb * 1024 * 1024);

  /* Change SQLite cache size */
  set_pragma_int("cache_size", FLAGS_num_pages);

  /* FLAGS_page_size is defaulted to 1024 */
  if (FLAGS_page_size != 1024)
    set_pragma_int("page_size", FLAGS_page_size);

  /* Change journal mode to WAL if WAL enabled flag is on */
  if (FLAGS_WAL_enabled) {
    set_pragma_str("journal_mode", "WAL");
    set_pragma_int("wal_autocheckpoint", FLAGS_checkpoint_granularity);
  } else {
    set_pragma_str("journal_mode", "OFF");
  }

  /* Change locking mode to exclusive and create tables/index for database */
  set_pragma_str("locking_mode", "EXCLUSIVE");

  char* create_stmt =
          "CREATE TABLE test (key blob, value blob, PRIMARY KEY (key))";
  status = sqlite3_exec(db_, create_stmt, NULL, NULL, &err_msg);
  exec_error_check(status, err_msg);

  stmt_prepare();
}

/* 
 *  This function is very simlar to benchmark_writebatch,
 *  but does do benchmark-related bookkeeping because it 
 *  is used to load the database beforehand.
 */
static void benchmark_prefill(int value_size, int entries) {
  char key[100];
  char *value;
  int status;
  int j, k;

  sqlite3_stmt *replace_stmt = stmts[STMT_REPLACE];
  /* Create and execute SQL statements */
  for (j = 0; j < entries; j++) {
    value = rand_gen_generate(&gen_, value_size);

    /* Create values for key-value pair */
    k = j;
    snprintf(key, sizeof(key), "%016d", k);

    /* Bind KV values into replace_stmt */
    status = sqlite3_bind_blob(replace_stmt, 1, key, 16, SQLITE_STATIC);
    error_check(status);
    status = sqlite3_bind_blob(replace_stmt, 2, value,
                                value_size, SQLITE_STATIC);
    error_check(status);

    /* Execute replace_stmt */
    status = sqlite3_step(replace_stmt);
    step_error_check(status);

    stmt_clear_and_reset(replace_stmt);
  }
}

static void benchmark_writebatch(int iter, int order, int num_entries, 
		int value_size, int entries_per_batch) {

  char key[100];
  char *value;
  int status;
  int j, k;

  sqlite3_stmt *replace_stmt = stmts[STMT_REPLACE];
  /* Create and execute SQL statements */
  for (j = 0; j < entries_per_batch; j++) {
    value = rand_gen_generate(&gen_, value_size);

    /* Create values for key-value pair */
    k = (order == SEQUENTIAL) ? iter + j :
                  (rand_next(&rand_) % num_entries);
    snprintf(key, sizeof(key), "%016d", k);

    /* Bind KV values into replace_stmt */
    status = sqlite3_bind_blob(replace_stmt, 1, key, 16, SQLITE_STATIC);
    error_check(status);
    status = sqlite3_bind_blob(replace_stmt, 2, value,
                                value_size, SQLITE_STATIC);
    error_check(status);

    /* Execute replace_stmt */
    bytes_ += value_size + strlen(key);
    status = sqlite3_step(replace_stmt);
    step_error_check(status);

    stmt_clear_and_reset(replace_stmt);
    finished_single_op();
  }
}

void warn_ops(int num_entries) {
  if (num_entries != num_) {
    char* msg = malloc(sizeof(char) * 100);
    snprintf(msg, 100, "(%d ops)", num_entries);
    message_ = msg;
  }
}

static void benchmark_write(bool write_sync, int order, int num_entries,
		int value_size, int entries_per_batch) {
  const bool synchronous = FLAGS_WAL_enabled || write_sync;
  const bool transaction = FLAGS_transaction;
  int i;

  warn_ops(num_entries);

  sqlite3_stmt *begin_trans_stmt = stmts[STMT_TSTART];
  sqlite3_stmt *end_trans_stmt = stmts[STMT_TEND];

  set_pragma_str("synchronous", (synchronous) ? "NORMAL" : "OFF");

  for (i = 0; i < num_entries; i += entries_per_batch) {
    /* Begin write transaction */
    if (transaction)
      stmt_runonce(begin_trans_stmt);

    benchmark_writebatch(i, order, num_entries, value_size, entries_per_batch);

    /* End write transaction */
    if (transaction)
      stmt_runonce(end_trans_stmt);
  }
}

static void benchmark_readbatch(int iter, int order, int entries_per_batch)
{
  sqlite3_stmt *read_stmt = stmts[STMT_READ];
  char key[100];
  int status;
  int j, k;

  /* Create and execute SQL statements */
  for (j = 0; j < entries_per_batch; j++) {
    /* Create key value */
    k = (order == SEQUENTIAL) ? iter + j : (rand_next(&rand_) % reads_);
    snprintf(key, sizeof(key), "%016d", k);

    /* Bind key value into read_stmt */
    status = sqlite3_bind_blob(read_stmt, 1, key, 16, SQLITE_STATIC);
    error_check(status);
    
    /* Execute read statement */
    while ((status = sqlite3_step(read_stmt)) == SQLITE_ROW) {}
    step_error_check(status);

    /* Reset SQLite statement for another use */
    stmt_clear_and_reset(read_stmt);
    finished_single_op();
  }
}

static void benchmark_read(int order, int entries_per_batch) {
  bool transaction = FLAGS_transaction && (entries_per_batch > 1);
  int i;

  sqlite3_stmt *begin_trans_stmt = stmts[STMT_TSTART];
  sqlite3_stmt *end_trans_stmt = stmts[STMT_TEND];

  for (i = 0; i < reads_; i += entries_per_batch) {
    /* Begin read transaction */
    if (transaction)
      stmt_runonce(begin_trans_stmt);

    benchmark_readbatch(i, order, entries_per_batch);

    /* End read transaction */
    if (transaction)
      stmt_runonce(end_trans_stmt);
  }
}

static void benchmark_readwrite(bool write_sync, int order, int num_entries,
                  int value_size, int entries_per_batch, int write_percent) {
  bool transaction = FLAGS_transaction;
  int i;

  warn_ops(num_entries);

  sqlite3_stmt *begin_trans_stmt = stmts[STMT_TSTART];
  sqlite3_stmt *end_trans_stmt = stmts[STMT_TEND];

  set_pragma_str("synchronous", (write_sync) ? "FULL" : "OFF");

  for (i = 0; i < num_entries; i += entries_per_batch) {
    /* Begin write transaction */
    if (transaction)
      stmt_runonce(begin_trans_stmt);

    if (rand_uniform(&rand_, 100) < write_percent)
    	benchmark_writebatch(i, order, num_entries, value_size, entries_per_batch);
    else
    	benchmark_readbatch(i, order, entries_per_batch);

    /* End write transaction */
    if (transaction)
      stmt_runonce(end_trans_stmt);
  }
}

void benchmark_init() {
  db_ = NULL;
  db_num_ = 0;
  num_ = FLAGS_num;
  reads_ = FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads;
  bytes_ = 0;
  rand_gen_init(&gen_, FLAGS_compression_ratio);
  rand_init(&rand_, 301);;

  struct dirent* ep;
  DIR* test_dir = opendir(FLAGS_db);
  if (!FLAGS_use_existing_db) {
    while ((ep = readdir(test_dir)) != NULL) {
      if (starts_with(ep->d_name, "dbbench_sqlite3")) {
        char file_name[1000];
        strcpy(file_name, FLAGS_db);
        strcat(file_name, ep->d_name);
        remove(file_name);
      }
    }
  }
  closedir(test_dir);
}

void benchmark_fini() {
  int status;

  stmt_finalize();
  status = sqlite3_close(db_);
  error_check(status);
}

int get_order(char *suffix) {
  return !strncmp(suffix, "seq", sizeof("seq") - 1) ? SEQUENTIAL : RANDOM;
}

int get_sync(char *name) {
  int len = sizeof("sync") - 1;
  return !strncmp(&name[strlen(name) - len], "sync", len);
}

int get_batch_size(char* name) {
  int len = sizeof("batch") - 1;
  return !strncmp(&name[strlen(name) - len], "batch", len) ? FLAGS_batch_size : 1;
}

void benchmark_run() {
  int batch_size, sync;
  char* benchmarks;
  char *suffix;

  print_header();
  benchmark_open();

  /* Prepopulate the database. */
  benchmark_prefill(num_ / 1000, 1000);

  benchmarks = FLAGS_benchmarks;
  while (benchmarks != NULL) {
    char* sep = strchr(benchmarks, ',');
    char* name;
    if (sep == NULL) {
      name = benchmarks;
      benchmarks = NULL;
    } else {
      name = calloc(sizeof(char), (sep - benchmarks + 1));
      strncpy(name, benchmarks, sep - benchmarks);
      benchmarks = sep + 1;
    }
    bytes_ = 0;
    /* Get the sync and batch size by checking the suffix of the benchmark. */
    sync = get_sync(name);
    batch_size = get_batch_size(name);

    start();

    /* Get the benchmark type and ordering by parsing the prefix of the name. */
    if (!strncmp(name, "fill", sizeof("fill") - 1)) {
      suffix = &name[sizeof("fill") - 1];
      benchmark_write(sync, get_order(suffix), num_, FLAGS_value_size, batch_size);
    } else if (!strncmp(name, "rw", sizeof("rw") - 1)) {
      suffix = &name[sizeof("rw") - 1];
      benchmark_readwrite(sync, get_order(suffix), num_, FLAGS_value_size, batch_size, FLAGS_write_percent);
    } else if (!strncmp(name, "read", sizeof("read") - 1)) {
      suffix = &name[sizeof("read") - 1];
      benchmark_read(get_order(suffix), 1);
    } else {
      if (strcmp(name, ""))
        fprintf(stderr, "unknown benchmark '%s'\n", name);
	  continue;
    }
    wal_checkpoint(db_);
    stop(name);
  }
}

