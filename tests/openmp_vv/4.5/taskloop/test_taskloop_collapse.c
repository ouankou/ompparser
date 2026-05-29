#pragma omp parallel num_threads(100)
#pragma omp single
#pragma omp taskloop collapse(2)
#pragma omp atomic
