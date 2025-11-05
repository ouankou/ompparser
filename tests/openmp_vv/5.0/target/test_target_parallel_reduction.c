#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: A[0:1024*32], B[0:1024*32], TotSum) device(gpu)
#pragma omp target parallel for reduction(+:TotSum) device(gpu)
#pragma omp target map (from: _ompvv_isOffloadingOn)
