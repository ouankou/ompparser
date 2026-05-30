#pragma omp parallel num_threads(500)
#pragma omp single
#pragma omp taskloop firstprivate(private_var)
