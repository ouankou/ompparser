#pragma omp parallel shared(x1,x2) num_threads(16)
#pragma omp taskloop
#pragma omp atomic
#pragma omp single
#pragma omp taskloop
#pragma omp atomic
