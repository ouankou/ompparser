#pragma omp parallel num_threads(8)
#pragma omp loop order(concurrent)
#pragma omp atomic update
