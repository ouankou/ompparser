#pragma omp parallel num_threads(100)
#pragma omp single
#pragma omp taskloop num_tasks(6)
#pragma omp atomic
