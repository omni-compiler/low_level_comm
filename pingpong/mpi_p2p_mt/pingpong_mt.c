#include "pingpong_common.h"
#include <omp.h>

int rank, nprocs, target;
char hostname[32];

int main(int argc, char **argv)
{
  const int required = MPI_THREAD_MULTIPLE;
  int count, iter, ret, provided;
  int num_loop = SMALL_NUM_LOOP;
  float *s_buf, *r_buf;
  double t_start = 0.0, t_end = 0.0, time;

  MPI_CHECK(MPI_Init_thread(&argc, &argv, required, &provided));
  assert(required <= provided);

  init_mpi_params(&rank, &nprocs, hostname);
  target = (rank + 1) % 2;
  assert(nprocs == 2);

#pragma omp parallel shared(time, rank, nprocs, target, hostname) firstprivate(required, provided, num_loop) \
  private(count, iter, ret, s_buf, r_buf)
  {
    const int tid = omp_get_thread_num();
    const int nthreads = omp_get_num_threads();
    const int tag = tid * 2;
    
    printf("rank = %d of %d ,thread = %d of %d, on %s, tag = %d, target = %d, required = %d, provided = %d\n",
	   rank, nprocs, tid, nthreads, hostname, tag, target, required, provided);

    ret = posix_memalign((void **)&s_buf, 4096, MAX_COUNT * sizeof(float));
    assert(ret == 0);
    ret = posix_memalign((void **)&r_buf, 4096, MAX_COUNT * sizeof(float));
    assert(ret == 0);

    for (count = 1; count <= MAX_COUNT; count *= 2) {
      size_t byte = count * sizeof(float);
      if (count > CHANGE_COUNT) {
	num_loop = LARGE_NUM_LOOP;
      }

#pragma omp master
      {
	time = 0.0;
      }

      init_buf(count, s_buf, rank);
      init_buf(count, r_buf, rank);

#pragma omp barrier
#pragma omp master
      {
	MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
      }
#pragma omp barrier

      for (iter = 0; iter < num_loop + SKIP; iter++) {
	if (iter == SKIP) {
	  t_start = MPI_Wtime();
	}

	if (rank == 0) {
	  MPI_CHECK(MPI_Send(s_buf, byte, MPI_BYTE, target, tag, MPI_COMM_WORLD));
	  MPI_CHECK(MPI_Recv(r_buf, byte, MPI_BYTE, target, tag+1, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
	} else {
	  MPI_CHECK(MPI_Recv(r_buf, byte, MPI_BYTE, target, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
	  MPI_CHECK(MPI_Send(s_buf, byte, MPI_BYTE, target, tag+1, MPI_COMM_WORLD));
	}
      } // end of iter
      t_end = MPI_Wtime();
      double t = t_end - t_start;

      int i;
#pragma omp for reduction(max:time)
      for (i = 0; i < 1; i++) {
	time = t;
      }

#pragma omp master
      {
	MPI_CHECK(MPI_Reduce(&time, &t, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD));
      }

      if (rank == 0 && tid == 0) {
	print_latency(byte, num_loop, t);
      }
    } // end of count

    free(s_buf);
    free(r_buf);
  } // end of omp parallel

  MPI_CHECK(MPI_Finalize());

  return 0;
}
