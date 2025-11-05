#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for allocate(x_alloc: x) private(x) shared(result) num_threads(8)
#pragma omp simd simdlen(16) aligned(x: 64)
