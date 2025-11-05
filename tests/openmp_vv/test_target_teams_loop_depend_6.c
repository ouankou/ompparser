#pragma omp target teams loop map(tofrom: b[0:1024], c[0:1024]) depend(in: b[0:1024])
