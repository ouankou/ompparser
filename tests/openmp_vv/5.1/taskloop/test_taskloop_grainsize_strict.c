#pragma omp parallel num_threads(8)
#pragma omp single
#pragma omp taskloop reduction(+: parallel_sum) grainsize(strict:1000)
