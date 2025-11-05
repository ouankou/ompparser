#pragma omp target map(from:array_copy[0:1000], sum_copy) defaultmap(tofrom:scalar)
