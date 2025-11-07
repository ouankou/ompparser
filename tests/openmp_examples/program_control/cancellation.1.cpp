#pragma omp parallel shared(ex)
#pragma omp for
#pragma omp atomic write
#pragma omp cancel for
#pragma omp cancel parallel
#pragma omp barrier
