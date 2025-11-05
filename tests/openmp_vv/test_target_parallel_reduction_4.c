#pragma omp target data map(tofrom: A[0:1024*32], B[0:1024*32], TotSum) device(gpu)
