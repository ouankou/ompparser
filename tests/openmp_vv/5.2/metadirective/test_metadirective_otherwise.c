#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp metadirective when(device = {kind(nohost)}: nothing) otherwise(target map(tofrom : on_host))
#pragma omp target map (from: _ompvv_isOffloadingOn)
