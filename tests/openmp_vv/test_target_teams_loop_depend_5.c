#pragma omp target teams loop map(tofrom: a[0:1024], b[0:1024]) depend(in: a[0:1024]) depend(out: b[0:1024]) nowait
