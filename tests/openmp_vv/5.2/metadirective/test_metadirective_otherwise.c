#pragma omp metadirective when(device = {kind(nohost)}: nothing) otherwise(target map(tofrom : on_host))
#pragma omp target map (from: _ompvv_isOffloadingOn)
