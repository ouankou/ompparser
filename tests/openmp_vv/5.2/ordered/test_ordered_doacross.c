#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for ordered
#pragma omp ordered doacross(sink: i-1)
#pragma omp ordered doacross(source:)
