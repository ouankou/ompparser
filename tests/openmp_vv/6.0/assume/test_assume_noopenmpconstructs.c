#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp assume no_openmp_constructs(0)
#pragma omp parallel for
#pragma omp assume no_openmp_constructs(1)
#pragma omp assume no_openmp_constructs(1)
#pragma omp assume no_openmp_constructs
