#pragma omp target teams distribute parallel for reduction(+:Total[0:1024]) private(j)
