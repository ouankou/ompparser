#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel loop bind(parallel) map(tofrom: arr) num_threads(8) private(j,k)
#pragma omp target map (from: _ompvv_isOffloadingOn)
