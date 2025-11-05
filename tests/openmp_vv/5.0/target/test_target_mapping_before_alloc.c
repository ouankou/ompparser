#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(alloc: scalar, a, member) map(to: scalar, a, member) map(tofrom: errors)
#pragma omp target map (from: _ompvv_isOffloadingOn)
