#ifndef PINGPONG_IBV_COMMON
#define PINGPONG_IBV_COMMON

#include "pingpong_common.h"
#include <infiniband/verbs.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

enum {
  PORT_NUM = 1,
};

enum {
  PINGPONG_RDMA_RECV_WRID = 1,
  PINGPONG_RDMA_SEND_WRID = 2,
};

struct ibv_comm_params {
  struct ibv_pd *pd;
  struct ibv_mr *s_mr;
  struct ibv_mr *r_mr;
  struct ibv_cq *send_cq;
  struct ibv_cq *recv_cq;
  struct ibv_qp *qp;
  uint16_t dest_lid;
  uint32_t dest_qp_num;
  uint64_t s_src_addr;
  uint64_t r_src_addr;
  uint32_t src_psn;
  uint32_t dest_psn;
};

static void modify_qp(struct ibv_comm_params *comm_params)
{
  int ret;
  struct ibv_qp *qp = comm_params->qp;
  uint32_t src_psn = comm_params->src_psn;
  uint16_t dest_lid = comm_params->dest_lid;
  uint32_t dest_pqn = comm_params->dest_qp_num;
  uint32_t dest_psn = comm_params->dest_psn;

  struct ibv_qp_attr *init_attr = (struct ibv_qp_attr *)malloc(sizeof(struct ibv_qp_attr));
  assert(init_attr != NULL);
  memset(init_attr, 0x00, sizeof(struct ibv_qp_attr));
  init_attr->qp_state = IBV_QPS_INIT;
  init_attr->port_num = PORT_NUM;
  init_attr->qp_access_flags = IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE;
  
  ret = ibv_modify_qp(qp, init_attr,
		      IBV_QP_STATE|IBV_QP_PKEY_INDEX|IBV_QP_PORT|IBV_QP_ACCESS_FLAGS);
  assert(ret == 0);

  struct ibv_qp_attr *rtr_attr = (struct ibv_qp_attr *)malloc(sizeof(struct ibv_qp_attr));
  assert(rtr_attr != NULL);
  memset(rtr_attr, 0x00, sizeof(struct ibv_qp_attr));
  rtr_attr->qp_state = IBV_QPS_RTR;
  rtr_attr->path_mtu = IBV_MTU_4096;
  rtr_attr->dest_qp_num = dest_pqn;
  rtr_attr->rq_psn = dest_psn;
  rtr_attr->max_dest_rd_atomic = 0;
  rtr_attr->min_rnr_timer = 0;
  rtr_attr->ah_attr.is_global = 0;
  rtr_attr->ah_attr.dlid = dest_lid;
  rtr_attr->ah_attr.sl = 0;
  rtr_attr->ah_attr.src_path_bits = 0;
  rtr_attr->ah_attr.port_num = PORT_NUM;

  ret = ibv_modify_qp(qp, rtr_attr,
		      IBV_QP_STATE|IBV_QP_AV|IBV_QP_PATH_MTU|IBV_QP_DEST_QPN|
		      IBV_QP_RQ_PSN|IBV_QP_MAX_DEST_RD_ATOMIC|IBV_QP_MIN_RNR_TIMER);
  assert(ret == 0);

  struct ibv_qp_attr *rts_attr = (struct ibv_qp_attr *)malloc(sizeof(struct ibv_qp_attr));
  assert(rts_attr != NULL);
  memset(rts_attr, 0x00, sizeof(struct ibv_qp_attr));
  rts_attr->qp_state = IBV_QPS_RTS;
  rts_attr->timeout = 0;
  rts_attr->retry_cnt = 7;
  rts_attr->rnr_retry = 7;
  rts_attr->sq_psn = src_psn;
  rts_attr->max_rd_atomic = 0;

  ret = ibv_modify_qp(qp, rts_attr,
		      IBV_QP_STATE|IBV_QP_TIMEOUT|IBV_QP_RETRY_CNT|IBV_QP_RNR_RETRY|IBV_QP_SQ_PSN|IBV_QP_MAX_QP_RD_ATOMIC);
  assert(ret == 0);
}

#endif
