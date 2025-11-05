#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target teams distribute if(attempt >= 70) map(tofrom: a)
#pragma omp target map (from: _ompvv_isOffloadingOn)
