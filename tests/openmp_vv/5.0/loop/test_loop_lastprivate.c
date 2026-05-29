#pragma omp parallel num_threads(8)
#pragma omp loop lastprivate(x)
#pragma omp parallel num_threads(8)
#pragma omp loop lastprivate(x, y) collapse(2)
