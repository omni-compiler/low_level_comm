#ifndef PINGPONG_COMMON
#define PINGPONG_COMMON

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>

#define MAX_COUNT 1024*1024
#define CHANGE_COUNT 32768
#define NUM_LOOP 1000
#define SMALL_NUM_LOOP 100000
#define LARGE_NUM_LOOP 1000
#define SKIP 100

#define MPI_CHECK(stmt)						\
  do {								\
    int mpi_errno = (stmt);					\
    if (MPI_SUCCESS != mpi_errno) {				\
      fprintf(stderr, "[%s:%d] MPI call failed with %d \n",     \
	      __FILE__, __LINE__,mpi_errno);			\
      exit(EXIT_FAILURE);                                       \
    }								\
    assert(MPI_SUCCESS == mpi_errno);				\
  } while (0)

static void init_mpi_params(int *rank, int *nprocs, char hostname[])
{
  int length;

  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, rank));
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, nprocs));
  MPI_CHECK(MPI_Get_processor_name(hostname, &length));
}

static void print_latency(const size_t size, const int loop, const double time)
{
  double latency = time / loop / 2 * 1e6; // usec
  printf("%zu\t%lf\t%lf\n", size, latency, (size / latency)); // Byte, usec, MB/s
}

static void init_buf(const int count, float *buf, int arg)
{
  int i;
  for (i = 0; i < count; i++) {
    buf[i] = (float)(i * 10 + arg);
  }
}

#endif
