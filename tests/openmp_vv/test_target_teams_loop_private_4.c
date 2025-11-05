#pragma omp target teams loop map(tofrom: b[0:1024]) private(a)
