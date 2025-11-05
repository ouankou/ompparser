#pragma omp target data map(tofrom: A[0:1024*1024], HostVar, errors) device(gpu)
