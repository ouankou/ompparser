#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for simd reduction(inscan, +: x) num_threads(8)
#pragma omp scan inclusive(x)
#pragma omp parallel for simd reduction(inscan, +: x) num_threads(8)
#pragma omp scan exclusive(x)
