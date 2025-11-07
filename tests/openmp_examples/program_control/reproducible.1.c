#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp threadprivate(sum)
#pragma omp parallel for order(concurrent)
#pragma omp parallel for ordered
#pragma omp ordered
#pragma omp parallel for copyin(sum)
#pragma omp parallel
