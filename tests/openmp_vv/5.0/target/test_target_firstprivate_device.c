#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: A[0:1024*1024], HostVar, errors) device(gpu)
#pragma omp target parallel for firstprivate(HostVar) device(gpu)
#pragma omp target map (from: _ompvv_isOffloadingOn)
