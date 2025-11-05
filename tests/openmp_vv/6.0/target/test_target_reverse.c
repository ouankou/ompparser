#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: arrayReverse[:1024])
#pragma omp reverse
#pragma omp target map (from: _ompvv_isOffloadingOn)
