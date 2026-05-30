#pragma omp parallel num_threads(1000)
#pragma omp single
#pragma omp taskloop lastprivate(val)
