#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(8) shared(a, b, num_threads, sum)
#pragma omp single
#pragma omp taskloop simd reduction(+:sum)
#pragma omp atomic
#pragma omp single
#pragma omp taskloop simd reduction(+:sum)
#pragma omp atomic
