#pragma omp barrier
#pragma omp parallel shared(k)
#pragma omp parallel private(i) shared(n)
#pragma omp for
