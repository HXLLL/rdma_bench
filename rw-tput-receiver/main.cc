#include "main.h"

DEFINE_uint64(num_threads, 0, "Number of threads");
DEFINE_uint64(is_client, 0, "Is this process a client?");
DEFINE_uint64(dual_port, 0, "Use two ports?");
DEFINE_uint64(use_uc, 0, "Use unreliable connected transport?");
DEFINE_uint64(do_read, 0, "Do RDMA reads?");
DEFINE_uint64(machine_id, 0, "ID of this machine");
DEFINE_uint64(size, 0, "RDMA size");
DEFINE_uint64(postlist, 0, "Postlist size");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  rt_assert(FLAGS_num_threads >= 1, "At least on thread needed");

  if (FLAGS_is_client == 1) {
    if (FLAGS_do_read == 0 && INLINE_WRITE) {
      rt_assert(FLAGS_size <= kHrdMaxInline, "Inline size too small");
    }
    rt_assert(FLAGS_postlist <= kAppMaxPostlist, "Postlist too large");
    rt_assert(kAppUnsigBatch >= FLAGS_postlist, "Postlist check failed");
    rt_assert(kHrdSQDepth >= 2 * kAppUnsigBatch, "Queue capacity check failed");
  }

  // Launch a single server thread or multiple client threads
  printf("main: Using %zu threads\n", FLAGS_num_threads);
  auto* param_arr = new thread_params_t[FLAGS_num_threads];
  std::vector<std::thread> thread_arr(FLAGS_num_threads);
  auto* tput = new double[FLAGS_num_threads];

  if (FLAGS_is_client == 1) {
    for (size_t i = 0; i < FLAGS_num_threads; i++) {
      param_arr[i].id = (FLAGS_machine_id * FLAGS_num_threads) + i;
      param_arr[i].tput = tput;

      thread_arr[i] = std::thread(run_client, &param_arr[i]);
    }

    for (auto& thread : thread_arr) thread.join();
  } else {
    param_arr[0].id = FLAGS_num_threads;
    thread_arr[0] = std::thread(run_server, &param_arr[0]);
    thread_arr[0].join();
  }

  return 0;
}
