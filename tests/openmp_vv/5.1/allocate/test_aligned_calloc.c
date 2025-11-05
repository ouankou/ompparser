#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for simd simdlen(16) aligned(x: 64)
#pragma omp parallel for simd simdlen(16) aligned(x: 64)
#pragma omp atomic write
