#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: x,y)
#pragma omp parallel
#pragma omp for order(reproducible:concurrent) nowait
#pragma omp for order(reproducible:concurrent)
