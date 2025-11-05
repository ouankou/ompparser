#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(8)
#pragma omp single
#pragma omp taskloop num_tasks(strict: 100) reduction(+: parallel_sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
