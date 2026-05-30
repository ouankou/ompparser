#pragma omp parallel num_threads(10)
#pragma omp single
#pragma omp taskloop final(100 == THRESHOLD)
#pragma omp task
#pragma omp task
#pragma omp task
