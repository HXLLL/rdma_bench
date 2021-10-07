#include "main.h"

void run_server(thread_params_t* params) {
  int nb_clients = params->id;

  size_t ib_port_index = 0;

  struct hrd_conn_config_t conn_config;
  conn_config.num_qps = params->id;
  conn_config.use_uc = (FLAGS_use_uc == 1);
  conn_config.prealloc_buf = nullptr;
  conn_config.buf_size = kAppBufSize;
  conn_config.buf_shm_key = -1;

  struct hrd_dgram_config_t dgram_config;
  dgram_config.num_qps = nb_clients;
  dgram_config.buf_size = 4096;
  dgram_config.buf_shm_key = -1;
  dgram_config.prealloc_buf = nullptr;

  int64_t sum=0;

  auto* cb = hrd_ctrl_blk_init(0, ib_port_index, kHrdInvalidNUMANode,
                              &conn_config, &dgram_config);

  for (size_t srv_gid=0; srv_gid<params->id; ++srv_gid) {
    size_t clt_gid = srv_gid;     // One-to-one connections

    char srv_name_conn[kHrdQPNameSize];
    sprintf(srv_name_conn, "server-conn-%zu", srv_gid);
    char srv_name_dgram[kHrdQPNameSize];
    sprintf(srv_name_dgram, "server-dgram-%zu", srv_gid);
    char clt_name_conn[kHrdQPNameSize];
    sprintf(clt_name_conn, "client-conn-%zu", clt_gid);
    char clt_name_dgram[kHrdQPNameSize];
    sprintf(clt_name_dgram, "client-dgram-%zu", clt_gid);

    hrd_publish_conn_qp(cb, srv_gid, srv_name_conn);
    hrd_publish_dgram_qp(cb, srv_gid, srv_name_dgram);
    printf("main: Server %s published. Waiting for client %s\n", srv_name_conn,
          clt_name_conn);
    printf("main: Server %s published. Waiting for client %s\n", srv_name_dgram,
          clt_name_dgram);
  }

  struct ibv_ah* ah[MAX_CLIENT];
  memset(ah, 0, MAX_CLIENT * sizeof(uintptr_t));
  struct hrd_qp_attr_t* clt_qp[MAX_CLIENT];

  for (int i = 0; i < nb_clients; i++) {
    char clt_name[kHrdQPNameSize];
    sprintf(clt_name, "client-dgram-%d", i);

    clt_qp[i] = NULL;
    while (clt_qp[i] == NULL) {
      clt_qp[i] = hrd_get_published_qp(clt_name);
      if (clt_qp[i] == NULL) {
        usleep(200000);
      }
    }

    printf("main: Worker found client %d of %d clients. Client LID: %d\n",
           i, nb_clients, clt_qp[i]->lid);

    struct ibv_ah_attr ah_attr;
    memset(&ah_attr, 0, sizeof(ibv_ah_attr));
    ah_attr.is_global = 0;
    ah_attr.dlid = clt_qp[i]->lid;
    ah_attr.sl = 0;
    ah_attr.src_path_bits = 0;
    ah_attr.port_num = 1;

    ah[i] = ibv_create_ah(cb->pd, &ah_attr);
    assert(ah[i] != NULL);
  }

  for (size_t srv_gid=0; srv_gid<params->id; ++srv_gid) {
    size_t clt_gid = srv_gid;     // One-to-one connections

    char srv_name[kHrdQPNameSize];
    sprintf(srv_name, "server-conn-%zu", srv_gid);
    char clt_name[kHrdQPNameSize];
    sprintf(clt_name, "client-conn-%zu", clt_gid);

    hrd_qp_attr_t* clt_qp = nullptr;
    while (clt_qp == nullptr) {
      clt_qp = hrd_get_published_qp(clt_name);
      if (clt_qp == nullptr) usleep(200000);
    }

    printf("main: Server %s found client! Connecting..\n", srv_name);
    hrd_connect_qp(cb, srv_gid, clt_qp);

    // This garbles the server's qp_attr - which is safe
    hrd_publish_ready(srv_name);
    printf("main: Server %s READY\n", srv_name);
  }

  volatile uint8_t *req_buf = cb->conn_buf;
  struct ibv_send_wr wr[MAX_CLIENT], *bad_send_wr = NULL;
  struct ibv_send_wr* first_send_wr;
  struct ibv_send_wr* last_send_wr;
  int ws[MAX_CLIENT] = {0};

  long long rolling_iter=0, nb_reports=0, nb_post_send=0, nb_tx_tot=0;

  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);

  while (true) {
    if (unlikely(rolling_iter >= M_4)) {
      clock_gettime(CLOCK_REALTIME, &end);
      double seconds = (end.tv_sec - start.tv_sec) +
                       static_cast<double>(end.tv_nsec - start.tv_nsec) / 1000000000;
      printf(
          "main: Worker: %.2f IOPS. Avg per-port postlist = %.2f. ",
          M_4 / seconds, static_cast<double>(nb_tx_tot) / nb_post_send );

      ++nb_reports;

      clock_gettime(CLOCK_REALTIME, &start);
    }

    int postlist_cnt = 0;

    for (int poll_i=0; poll_i<nb_clients; ++poll_i) {
      int req_offset = OFFSET(FLAGS_size, poll_i, ws[poll_i]);

      if (*(const_cast<uint8_t*>(req_buf)+req_offset) > 0) {
        *(const_cast<uint8_t*>(req_buf) + req_offset) = 0;
        if (postlist_cnt == 0) {
          first_send_wr = last_send_wr = &wr[postlist_cnt];
        } else {
          last_send_wr->next = &wr[postlist_cnt];
          last_send_wr = &wr[postlist_cnt];
        }

        // forge wr
        // ...

      }
    }

    // send postlist wr
    // ...

  }
}
