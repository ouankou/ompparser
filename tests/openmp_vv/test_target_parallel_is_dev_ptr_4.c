#pragma omp target data map(tofrom: Hst_A[0:1024]) device(gpu)
