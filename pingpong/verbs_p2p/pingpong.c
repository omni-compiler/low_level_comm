#include "pingpong_ibv_common.h"

int rank, nprocs, target;
char hostname[32];

static void post_recv(struct ibv_comm_params *comm_params, size_t length);
static void poll_recv(struct ibv_comm_params *comm_params);
static void post_send(struct ibv_comm_params *comm_params, size_t length);
static void poll_send(struct ibv_comm_params *comm_params);

int main(int argc, char **argv)
{
  int count, iter, ret;
  int num_loop = SMALL_NUM_LOOP;
  float *s_buf, *r_buf;
  double t_start = 0.0, t_end = 0.0;

  MPI_CHECK(MPI_Init(&argc, &argv));

  init_mpi_params(&rank, &nprocs, hostname);
  target = (rank + 1) % 2;
  assert(nprocs == 2);

  printf("rank = %d of %d on %s, target = %d\n", rank, nprocs, hostname, target);

  ret = posix_memalign((void **)&s_buf, 4096, MAX_COUNT * sizeof(float));
  assert(ret == 0);
  ret = posix_memalign((void **)&r_buf, 4096, MAX_COUNT * sizeof(float));
  assert(ret == 0);

  // Initialize IB device
  struct ibv_device **dev_list;
  dev_list = ibv_get_device_list(NULL);

  if (!dev_list) {
    int errsave = errno;
    fprintf(stderr, "Failure: ibv_get_device_list (errno=%d)\n", errsave);
    MPI_CHECK(MPI_Finalize());
    exit(EXIT_FAILURE);        
  }

  struct ibv_device *device = dev_list[0];
  fprintf(stdout, "[%d of %d] IB device: %s, GUID: %016" PRIx64 "\n",
	  rank, nprocs, ibv_get_device_name(device), ibv_get_device_guid(device));
  fflush(stdout);

  struct ibv_context *context = NULL;
  context = ibv_open_device(device);
  assert(context);

  struct ibv_port_attr port_attr;
  ret = ibv_query_port(context, PORT_NUM, &port_attr);
  assert(ret == 0 && port_attr.lid > 0);

  struct ibv_comm_params *comm_params = (struct ibv_comm_params *)malloc(sizeof(struct ibv_comm_params));
  assert(comm_params != NULL);

  struct ibv_qp_init_attr *qp_init_attr = (struct ibv_qp_init_attr *)malloc(sizeof(struct ibv_qp_init_attr));
  assert(qp_init_attr != NULL);

  memset(comm_params, 0x00, sizeof(struct ibv_comm_params));
  MPI_CHECK(MPI_Sendrecv(&(port_attr.lid), sizeof(uint16_t), MPI_BYTE, target, 0,
			 &(comm_params->dest_lid), sizeof(uint16_t), MPI_BYTE, target, 0,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE));

  comm_params->pd = ibv_alloc_pd(context);
  assert(comm_params->pd);

  comm_params->send_cq = ibv_create_cq(context, 1024, NULL, NULL, 0);
  assert(comm_params->send_cq);

  comm_params->recv_cq = ibv_create_cq(context, 1024, NULL, NULL, 0);
  assert(comm_params->recv_cq);

  comm_params->s_mr = ibv_reg_mr(comm_params->pd, s_buf, MAX_COUNT * sizeof(float), IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE);
  assert(comm_params->s_mr);

  comm_params->r_mr = ibv_reg_mr(comm_params->pd, r_buf, MAX_COUNT * sizeof(float), IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE);
  assert(comm_params->r_mr);

  memset(qp_init_attr, 0x00, sizeof(struct ibv_qp_init_attr));
  qp_init_attr->qp_type = IBV_QPT_RC;
  qp_init_attr->send_cq = comm_params->send_cq;
  qp_init_attr->recv_cq = comm_params->recv_cq;
  qp_init_attr->cap.max_send_wr = 1;
  qp_init_attr->cap.max_recv_wr = 32;
  qp_init_attr->cap.max_send_sge = 1;
  qp_init_attr->cap.max_recv_sge = 1;
  qp_init_attr->sq_sig_all = 1;

  comm_params->qp = ibv_create_qp(comm_params->pd, qp_init_attr);
  assert(comm_params->qp);

  comm_params->src_psn = (rand() & 0xFFFFFF);
  MPI_CHECK(MPI_Sendrecv(&(comm_params->src_psn), sizeof(uint32_t), MPI_BYTE, target, 1,
			 &(comm_params->dest_psn), sizeof(uint32_t), MPI_BYTE, target, 1,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE));

  MPI_CHECK(MPI_Sendrecv(&(comm_params->qp->qp_num), sizeof(uint32_t), MPI_BYTE, target, 2,
			 &(comm_params->dest_qp_num), sizeof(uint32_t), MPI_BYTE, target, 2,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE));

  comm_params->s_src_addr = (uint64_t)(s_buf);
  comm_params->r_src_addr = (uint64_t)(r_buf);

  modify_qp(comm_params);
  // end of initializing IB device

  for (count = 1; count <= MAX_COUNT; count *= 2) {
    size_t byte = count * sizeof(float);
    if (count > CHANGE_COUNT) {
      num_loop = LARGE_NUM_LOOP;
    }
      
    init_buf(count, s_buf, rank);
    init_buf(count, r_buf, rank);

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    for (iter = 0; iter < num_loop + SKIP; iter++) {
      if (iter == SKIP) {
      	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
      	t_start = MPI_Wtime();
      }

      if (rank == 0) {
	post_send(comm_params, byte);
	poll_send(comm_params);

	post_recv(comm_params, MAX_COUNT * sizeof(float));
	poll_recv(comm_params);
      } else {
	post_recv(comm_params, MAX_COUNT * sizeof(float));
	poll_recv(comm_params);

	post_send(comm_params, byte);
	poll_send(comm_params);
      }
    } // end of iter
    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
    t_end = MPI_Wtime();

    double t = t_end - t_start;
    if (rank == 0) {
      print_latency(byte, num_loop, t);
    }
  } // end of count

  ibv_destroy_qp(comm_params->qp);
  ibv_destroy_cq(comm_params->send_cq);
  ibv_destroy_cq(comm_params->recv_cq);
  ibv_dereg_mr(comm_params->s_mr);
  ibv_dereg_mr(comm_params->r_mr);
  ibv_dealloc_pd(comm_params->pd);

  free(qp_init_attr);
  free(comm_params);

  ibv_close_device(context);

  ibv_free_device_list(dev_list);

  free(s_buf);
  free(r_buf);

  MPI_CHECK(MPI_Finalize());

  return 0;
}

static void post_recv(struct ibv_comm_params *comm_params, size_t length)
{
  int i;
  struct ibv_qp *qp = comm_params->qp;
  struct ibv_mr *mr = comm_params->r_mr;
  uint64_t src_addr = comm_params->r_src_addr;
  struct ibv_recv_wr *bad_wr;

  struct ibv_sge *sge = (struct ibv_sge *)malloc(sizeof(struct ibv_sge));
  assert(sge != NULL);
  struct ibv_recv_wr *recv_wr = (struct ibv_recv_wr *)malloc(sizeof(struct ibv_recv_wr));
  assert(recv_wr != NULL);

  memset(sge, 0x00, sizeof(struct ibv_sge));
  sge->addr = src_addr;
  sge->length = length;
  sge->lkey = mr->lkey;
  
  memset(recv_wr, 0x00, sizeof(struct ibv_recv_wr));
  recv_wr->wr_id = PINGPONG_RDMA_RECV_WRID;
  recv_wr->next = NULL;
  recv_wr->sg_list = sge;
  recv_wr->num_sge = 1;

  for (i = 0; i < 32; i++) {
    if (ibv_post_recv(qp, recv_wr, &bad_wr))
      break;
  }
}

static void poll_recv(struct ibv_comm_params *comm_params)
{
  int ret;
  struct ibv_wc wc;
  int num_wr = 1;
  struct ibv_cq *cq = comm_params->recv_cq;

  while (num_wr > 0) {
    ret = ibv_poll_cq(cq, 1, &wc);

    if (ret == 0)
      continue; /* polling */

    if (ret < 0) {
      fprintf(stderr, "Failure: ibv_poll_cq\n");
      exit(EXIT_FAILURE);
    }
        
    if (wc.status != IBV_WC_SUCCESS) {
      fprintf(stderr, "[%d of %d] Completion errror: %s\n", rank, nprocs, ibv_wc_status_str(wc.status));
      exit(EXIT_FAILURE);
    }
    
    if (wc.opcode == IBV_WC_RECV) {
      /* fprintf(stdout, "[%d of %d] poll recv wc: wr_id=0x%016" PRIx64 " byte_len=%u, imm_data=0x%016" PRIx32 "\n", */
      /* 	      rank, nprocs, wc.wr_id, wc.byte_len, wc.imm_data); */
    } else {
      exit(EXIT_FAILURE);
    }

    num_wr--;
  }
}

static void post_send(struct ibv_comm_params *comm_params, size_t length)
{
  int ret;
  struct ibv_qp *qp = comm_params->qp;
  struct ibv_mr *mr = comm_params->s_mr;
  uint64_t src_addr = comm_params->s_src_addr;

  struct ibv_sge *sge = (struct ibv_sge *)malloc(sizeof(struct ibv_sge));
  assert(sge != NULL);
  struct ibv_send_wr *send_wr = (struct ibv_send_wr *)malloc(sizeof(struct ibv_send_wr));
  assert(send_wr != NULL);
  struct ibv_send_wr *bad_wr;
  
  memset(sge, 0x00, sizeof(struct ibv_sge));
  sge->addr = src_addr;
  sge->length = length;
  sge->lkey = mr->lkey;

  memset(send_wr, 0x00, sizeof(struct ibv_send_wr));
  send_wr->wr_id = PINGPONG_RDMA_SEND_WRID;
  send_wr->next = NULL;
  send_wr->sg_list = sge;
  send_wr->num_sge = 1;
  send_wr->opcode = IBV_WR_SEND;
  
  ret = ibv_post_send(qp, send_wr, &bad_wr);
  assert(ret == 0);
}

static void poll_send(struct ibv_comm_params *comm_params)
{
  int ret;
  struct ibv_wc wc;
  int num_wr = 1;
  struct ibv_cq *cq = comm_params->send_cq;

  while (num_wr > 0) {
    ret = ibv_poll_cq(cq, 1, &wc);

    if (ret == 0)
      continue; /* polling */

    if (ret < 0) {
      fprintf(stderr, "Failure: ibv_poll_cq\n");
      exit(EXIT_FAILURE);
    }
        
    if (wc.status != IBV_WC_SUCCESS) {
      fprintf(stderr, "[%d of %d] Completion errror: %s\n", rank, nprocs, ibv_wc_status_str(wc.status));
      exit(EXIT_FAILURE);
    }

    if (wc.opcode == IBV_WC_SEND) {
      /* fprintf(stdout, "[%d of %d] poll send wc: wr_id=0x%016" PRIx64 "\n", rank, nprocs, wc.wr_id); */
    } else {
      fprintf(stderr, "Opcode error\n");
      exit(EXIT_FAILURE);
    }

    num_wr--;
  }
}
