#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(2)
#pragma omp for collapse(2) ordered private(j,k) schedule(static,3)
#pragma omp ordered
