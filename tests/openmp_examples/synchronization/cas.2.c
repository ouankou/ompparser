#pragma omp parallel for num_threads(2)
#pragma omp atomic read
#pragma omp atomic compare capture
