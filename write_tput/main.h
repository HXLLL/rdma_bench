#ifndef MAIN_H
#define MAIN_H

#include <gflags/gflags.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>
#include "libhrd_cpp/hrd.h"

static constexpr size_t kAppBufSize = (8 * 1024 * 1024);
static constexpr size_t kAppMaxPostlist = 64;
static constexpr size_t kAppUnsigBatch = 64;

#define INLINE_WRITE 0
#define MAX_CLIENT 256
#define REQ_BUFFER_SIZE 128
#define OFFSET(size, cn, ws) \
  ((((cn) * REQ_BUFFER_SIZE) + (ws)) * (size))

struct thread_params_t {
  size_t id;
  double* tput;
};

DECLARE_uint64(num_threads);
DECLARE_uint64(is_client);
DECLARE_uint64(dual_port);
DECLARE_uint64(use_uc);
DECLARE_uint64(do_read);
DECLARE_uint64(machine_id);
DECLARE_uint64(size);
DECLARE_uint64(postlist);

void run_server(thread_params_t* params);
void run_client(thread_params_t* params);
#endif