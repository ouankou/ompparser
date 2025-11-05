#pragma omp target data map(tofrom: A, B) device(gpu)
