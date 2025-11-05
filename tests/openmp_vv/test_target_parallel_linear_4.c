#pragma omp target data map(tofrom: A[0:1024], val) device(gpu)
