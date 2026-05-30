#pragma omp parallel num_threads(100)
#pragma omp single
#pragma omp taskloop private(private_var)
#pragma omp atomic
