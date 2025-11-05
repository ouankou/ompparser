#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp task
#pragma omp critical
#pragma omp parallel
#pragma omp task
#pragma omp critical
#pragma omp critical
