#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task in_reduction(+:sum)
#pragma omp taskwait
#pragma omp task in_reduction(+:sum)
#pragma omp taskwait
#pragma omp taskgroup task_reduction(+:sum)
#pragma omp taskloop reduction(+:sum)
