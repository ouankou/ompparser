#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target teams distribute parallel for num_threads(8)
#pragma omp parallel for num_threads(8)
#pragma omp target teams distribute parallel for if(parallel: attempt >= 70) map(tofrom: a, warning) num_threads(8)
#pragma omp target map (from: _ompvv_isOffloadingOn)
