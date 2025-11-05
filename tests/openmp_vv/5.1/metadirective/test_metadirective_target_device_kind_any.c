#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp metadirective when( target_device={kind(any)}: target defaultmap(none) map(tofrom: A)) default( target defaultmap(none) map(to: A))
#pragma omp target map (from: _ompvv_isOffloadingOn)
