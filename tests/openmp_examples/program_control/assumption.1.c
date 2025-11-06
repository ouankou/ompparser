#pragma omp declare target
#pragma omp end declare target
#pragma omp begin assumes no_parallelism
#pragma omp end assumes
#pragma omp target teams distribute parallel for map(tofrom: A[0:N])
#pragma omp assume holds (N % 8 == 0 && N > 0)
#pragma omp simd
