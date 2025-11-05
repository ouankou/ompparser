#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(10)
#pragma omp single
#pragma omp taskloop final(100 == THRESHOLD)
#pragma omp task
#pragma omp task
#pragma omp task
