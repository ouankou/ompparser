#pragma omp target data map(tofrom: array[0:1000]) device(dev)
