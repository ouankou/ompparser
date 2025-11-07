#pragma omp target
#pragma omp target data map(sp[0])
#pragma omp target
#pragma omp target data map(s)
#pragma omp target
#pragma omp declare target enter(ss)
#pragma omp target map(always,from:ss)
