#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel num_threads(8) map(tofrom: a, b, x)
#pragma omp loop lastprivate(x)
#pragma omp target parallel num_threads(8) map(tofrom: a, b, x, y)
#pragma omp loop lastprivate(x, y) collapse(2)
#pragma omp target map (from: _ompvv_isOffloadingOn)
