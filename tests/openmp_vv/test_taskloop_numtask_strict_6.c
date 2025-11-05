#pragma omp taskloop num_tasks(strict: 100) reduction(+: parallel_sum)
