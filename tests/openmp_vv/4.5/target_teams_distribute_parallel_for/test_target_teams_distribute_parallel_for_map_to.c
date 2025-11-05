#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for map(to: a, b, scalar) map(tofrom: d)
#pragma omp target map (from: _ompvv_isOffloadingOn)
