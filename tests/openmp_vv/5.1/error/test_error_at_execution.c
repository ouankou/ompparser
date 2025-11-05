#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp single
#pragma omp error at(execution) severity(warning)
#pragma omp target map (from: _ompvv_isOffloadingOn)
