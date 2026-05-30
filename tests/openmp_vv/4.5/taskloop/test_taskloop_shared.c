#pragma omp parallel num_threads(50)
#pragma omp single
#pragma omp taskloop shared(shared_var)
#pragma omp atomic
