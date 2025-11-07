#pragma omp single copyprivate(tmp)
#pragma omp masked
#pragma omp barrier
#pragma omp barrier
#pragma omp single nowait
