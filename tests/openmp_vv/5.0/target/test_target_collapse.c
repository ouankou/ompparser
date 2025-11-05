#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: A, B) device(gpu)
#pragma omp target parallel for collapse(2) shared(A, B) device(gpu)
#pragma omp atomic
#pragma omp target map (from: _ompvv_isOffloadingOn)
