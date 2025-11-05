#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for collapse(4) map(tofrom: a) private(i,j,k,l)
#pragma omp target map (from: _ompvv_isOffloadingOn)
