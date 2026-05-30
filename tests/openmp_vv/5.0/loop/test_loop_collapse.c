#pragma omp parallel num_threads(8)
#pragma omp loop collapse(1)
#pragma omp parallel num_threads(8)
#pragma omp loop collapse(2)
