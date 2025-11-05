#pragma omp taskloop reduction(+: parallel_sum) grainsize(strict:1000)
