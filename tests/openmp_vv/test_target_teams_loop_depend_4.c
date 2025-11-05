#pragma omp target teams loop map(tofrom: a[0:1024]) depend(out: a[0:1024]) nowait
