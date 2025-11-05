#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires dynamic_allocators
#pragma omp target map(from: saved_x)
#pragma omp parallel for simd simdlen(16) aligned(x: 64)
#pragma omp target map (from: _ompvv_isOffloadingOn)
