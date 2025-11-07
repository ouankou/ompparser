#pragma omp parallel default(shared)
#pragma omp for
#pragma omp parallel shared(i, n)
#pragma omp for
