#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp for simd simdlen(16) aligned(x,y:64)
#pragma omp for simd simdlen(16) aligned(x,y:64)
#pragma omp target map (from: _ompvv_isOffloadingOn)
