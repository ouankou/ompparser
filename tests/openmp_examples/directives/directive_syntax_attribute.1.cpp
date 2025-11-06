#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare simd linear(i) simdlen(4)
#pragma omp declare simd linear(i) simdlen(8)
#pragma omp parallel for num_threads(4)
#pragma omp parallel num_threads(4)
#pragma omp for
#pragma omp simd reduction(+:tmp) simdlen(4)
#pragma omp simd reduction(+:tmp) simdlen(8)
#pragma omp simd reduction(+:tmp) simdlen(4)
#pragma omp simd reduction(+:tmp) simdlen(8)
