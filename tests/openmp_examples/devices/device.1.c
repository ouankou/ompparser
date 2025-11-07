#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target device(42) map(p[:N], v1[:N], v2[:N])
#pragma omp parallel for private(i) num_threads(nthreads)
