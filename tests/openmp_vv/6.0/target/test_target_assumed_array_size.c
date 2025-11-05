#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: array[0:1024])
#pragma omp target parallel for map(pointer[:])
#pragma omp target map (from: _ompvv_isOffloadingOn)
