#pragma omp parallel shared(a) private(i)
#pragma omp masked
#pragma omp for reduction(+:a)
#pragma omp single
