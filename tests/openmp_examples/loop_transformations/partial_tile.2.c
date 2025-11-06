#pragma omp parallel for schedule(static)
#pragma omp tile sizes(4,16)
#pragma omp parallel for schedule(static)
#pragma omp parallel
#pragma omp for schedule(static) nowait
#pragma omp for schedule(static)
