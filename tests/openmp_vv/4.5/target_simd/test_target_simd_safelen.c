#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target simd safelen(1) map(tofrom: A[0:1024])
#pragma omp target simd safelen(5) map(tofrom: A[0:1024])
#pragma omp target simd safelen(8) map(tofrom: A[0:1024])
#pragma omp target simd safelen(13) map(tofrom: A[0:1024])
#pragma omp target simd safelen(16) map(tofrom: A[0:1024])
#pragma omp target simd safelen(100) map(tofrom: A[0:1024])
#pragma omp target simd safelen(128) map(tofrom: A[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
