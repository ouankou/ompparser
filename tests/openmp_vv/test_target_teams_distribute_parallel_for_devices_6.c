#pragma omp target exit data map(from: a[0:1024]) device(dev)
