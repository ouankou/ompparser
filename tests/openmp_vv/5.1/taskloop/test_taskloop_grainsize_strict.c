#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(8)
#pragma omp single
#pragma omp taskloop reduction(+: parallel_sum) grainsize(strict:1000)
