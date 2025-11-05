#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for reduction(+:Total)
#pragma omp target teams distribute parallel for reduction(+:Total[0:1024]) private(j)
#pragma omp target map (from: _ompvv_isOffloadingOn)
