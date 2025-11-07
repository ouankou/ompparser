#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp threadprivate(x)
#pragma omp parallel default(none) private(a) shared(z, c)
#pragma omp for firstprivate(y)
