#include "pingpong_common.h"

int rank, nprocs, target;
char hostname[32];

int main(int argc, char **argv)
{
  int count, iter, ret;
  int num_loop = SMALL_NUM_LOOP;
  float *s_buf, *r_buf;
  double t_start = 0.0, t_end = 0.0;

  MPI_CHECK(MPI_Init(&argc, &argv));

  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &nprocs));
  MPI_CHECK(MPI_Get_processor_name(hostname, &count));
  target = (rank + 1) % 2;
  assert(nprocs == 2);

  printf("rank = %d of %d on %s, target = %d\n", rank, nprocs, hostname, target);

  ret = posix_memalign((void **)&s_buf, 4096, MAX_COUNT * sizeof(float));
  assert(ret == 0);
  ret = posix_memalign((void **)&r_buf, 4096, MAX_COUNT * sizeof(float));
  assert(ret == 0);

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
        MPI_Barrier(MPI_COMM_WORLD);
        t_start = MPI_Wtime();
      }

      if (rank == 0) {
        MPI_CHECK(MPI_Send(s_buf, byte, MPI_BYTE, target, 0, MPI_COMM_WORLD));
        MPI_CHECK(MPI_Recv(r_buf, byte, MPI_BYTE, target, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
      } else {
        MPI_CHECK(MPI_Recv(r_buf, byte, MPI_BYTE, target, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        MPI_CHECK(MPI_Send(s_buf, byte, MPI_BYTE, target, 1, MPI_COMM_WORLD));
      }
    } // end of iter
    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
    t_end = MPI_Wtime();

    double t = t_end - t_start;
    if (rank == 0) {
      print_latency(byte, num_loop, t);
    }
  } // end of count

  free(s_buf);
  free(r_buf);

  MPI_CHECK(MPI_Finalize());

  return 0;
}
