#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target simd simdlen(1) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(5) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(8) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(13) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(16) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(100) map(tofrom: A[0:1024])
#pragma omp target simd simdlen(128) map(tofrom: A[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
