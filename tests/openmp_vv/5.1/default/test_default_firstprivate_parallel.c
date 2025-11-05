#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel default(firstprivate) num_threads(8)
#pragma omp target map (from: _ompvv_isOffloadingOn)
