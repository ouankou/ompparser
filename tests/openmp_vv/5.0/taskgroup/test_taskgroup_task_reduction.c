#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp taskgroup task_reduction(+: result)
#pragma omp task in_reduction(+: result)
#pragma omp parallel shared(result) num_threads(8)
#pragma omp single
