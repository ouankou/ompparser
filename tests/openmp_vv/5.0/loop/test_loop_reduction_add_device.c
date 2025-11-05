#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel num_threads(8) map(tofrom: total, a, b, num_threads)
#pragma omp loop reduction(+:total)
#pragma omp for
#pragma omp target map (from: _ompvv_isOffloadingOn)
