#pragma omp target teams loop defaultmap(tofrom:scalar) reduction(+:x)
