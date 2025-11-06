#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp masked
#pragma omp parallel
#pragma omp parallel
#pragma omp parallel
#pragma omp parallel num_threads(8)
#pragma omp parallel
#pragma omp parallel
#pragma omp parallel num_threads(8,2)
#pragma omp parallel
#pragma omp parallel
