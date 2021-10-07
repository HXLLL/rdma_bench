#include "main.h"

void run_client(thread_params_t* params) {
  size_t clt_gid = params->id;  // Global ID of this client thread
  size_t srv_gid = clt_gid;     // One-to-one connections
  size_t clt_lid = params->id % FLAGS_num_threads;  // Local ID of this client
  size_t ib_port_index = FLAGS_dual_port == 0 ? 0 : (srv_gid % 2) * 2;
  uint64_t myseed = clt_gid;

  hrd_conn_config_t conn_config;
  conn_config.num_qps = 1;
  conn_config.use_uc = (FLAGS_use_uc == 1);
  conn_config.prealloc_buf = nullptr;
  conn_config.buf_size = kAppBufSize;
  conn_config.buf_shm_key = -1;

  hrd_dgram_config_t dgram_config;
  dgram_config.num_qps = 1;
  dgram_config.prealloc_buf = nullptr;
  dgram_config.buf_size = 4096;
  dgram_config.buf_shm_key = -1;

  auto* cb = hrd_ctrl_blk_init(clt_gid, ib_port_index, kHrdInvalidNUMANode,
                               &conn_config, &dgram_config);

  memset(const_cast<uint8_t*>(cb->conn_buf), 0, kAppBufSize);

  char srv_name_conn[kHrdQPNameSize];
  sprintf(srv_name_conn, "server-conn-%zu", srv_gid);
  char srv_name_dgram[kHrdQPNameSize];
  sprintf(srv_name_dgram, "server-dgram-%zu", srv_gid);
  char clt_name_conn[kHrdQPNameSize];
  sprintf(clt_name_conn, "client-conn-%zu", clt_gid);
  char clt_name_dgram[kHrdQPNameSize];
  sprintf(clt_name_dgram, "client-dgram-%zu", clt_gid);

  hrd_publish_conn_qp(cb, 0, clt_name_conn);
  hrd_publish_dgram_qp(cb, 0, clt_name_dgram);
  printf("main: Client %s published. Waiting for server %s\n", clt_name_conn,
         srv_name_conn);
  printf("main: Client %s published. Waiting for server %s\n", clt_name_dgram,
         srv_name_dgram);

  hrd_qp_attr_t* srv_qp = nullptr;
  while (srv_qp == nullptr) {
    srv_qp = hrd_get_published_qp(srv_name_conn);
    if (srv_qp == nullptr) usleep(200000);
  }

  printf("main: Client %s found server. Connecting..\n", clt_name_conn);
  hrd_connect_qp(cb, 0, srv_qp);
  printf("main: Client %s connected!\n", clt_name_conn);

  hrd_wait_till_ready(srv_name_conn);

  struct ibv_send_wr wr[kAppMaxPostlist], *bad_send_wr;
  struct ibv_sge sgl[kAppMaxPostlist];
  struct ibv_wc wc;
  size_t rolling_iter = 0;  // For performance measurement
  size_t nb_tx = 0;         // For selective signaling
  int ret;

  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);

  // The reads/writes at different postlist positions should be done to/from
  // different cache lines.
  size_t stride = 64;
  while (stride < FLAGS_size) stride += 64;
  rt_assert(stride * FLAGS_postlist <= kAppBufSize);

  auto opcode = FLAGS_do_read == 0 ? IBV_WR_RDMA_WRITE : IBV_WR_RDMA_READ;
  int cnt_rnd = 0;
  int ws = 0, server_ws = 0;

  while (true) {
    if (rolling_iter >= KB(512)) {
      clock_gettime(CLOCK_REALTIME, &end);
      double seconds = (end.tv_sec - start.tv_sec) +
                       (end.tv_nsec - start.tv_nsec) / 1000000000.0;
      double tput_mrps = rolling_iter / (seconds * 1000000);
      // printf("main: Client %zu: %.2f M/s\n", clt_gid, tput_mrps);
      rolling_iter = 0;

      // Per-machine stats
      params->tput[clt_lid] = tput_mrps;
      if (clt_lid == 0) {
        double tot = 0;
        for (size_t i = 0; i < FLAGS_num_threads; i++) tot += params->tput[i];
        hrd_red_printf("main: Machine: %.2f M/s\n", tot);
      }

      clock_gettime(CLOCK_REALTIME, &start);
    }

    // Post a batch
    for (size_t w_i = 0; w_i < FLAGS_postlist; w_i++) {
      wr[w_i].opcode = opcode;
      wr[w_i].num_sge = 1;
      wr[w_i].next = (w_i == FLAGS_postlist - 1) ? nullptr : &wr[w_i + 1];
      wr[w_i].sg_list = &sgl[w_i];

      wr[w_i].send_flags = nb_tx % kAppUnsigBatch == 0 ? IBV_SEND_SIGNALED : 0;
      if (nb_tx % kAppUnsigBatch == 0 && nb_tx > 0) {
        hrd_poll_cq(cb->conn_cq[0], 1, &wc);
      }

      wr[w_i].send_flags |= FLAGS_do_read == 0 && INLINE_WRITE ? IBV_SEND_INLINE : 0;

      sgl[w_i].addr = reinterpret_cast<uint64_t>(&cb->conn_buf[stride * w_i]);
      sgl[w_i].length = FLAGS_size;
      sgl[w_i].lkey = cb->conn_buf_mr->lkey;

      wr[w_i].wr.rdma.remote_addr =
          (uint64_t)(srv_qp->buf_addr + OFFSET(FLAGS_size, clt_gid, w_i));
      wr[w_i].wr.rdma.rkey = srv_qp->rkey;

      HRD_MOD_ADD(ws, REQ_BUFFER_SIZE);
      nb_tx++;
    }

    ret = ibv_post_send(cb->conn_qp[0], &wr[0], &bad_send_wr);
    rt_assert(ret == 0);

    rolling_iter += FLAGS_postlist;
  }
}
