#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target to (compute_on_device)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp parallel master
#pragma omp taskgroup task_reduction(+:sum)
#pragma omp target in_reduction(+:sum)
#pragma omp task in_reduction(+:sum)
