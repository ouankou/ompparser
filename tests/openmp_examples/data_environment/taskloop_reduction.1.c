#pragma omp taskloop reduction(+: res)
#pragma omp parallel
#pragma omp single
