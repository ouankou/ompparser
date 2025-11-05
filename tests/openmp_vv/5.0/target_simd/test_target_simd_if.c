#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target simd simdlen(64) if(k == 1024)
#pragma omp target simd simdlen(64) if(k != 1024)
#pragma omp target map (from: _ompvv_isOffloadingOn)
