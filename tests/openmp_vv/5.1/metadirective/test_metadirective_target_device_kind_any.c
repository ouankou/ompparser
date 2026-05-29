#pragma omp metadirective when( target_device={kind(any)}: target defaultmap(none) map(tofrom: A)) default( target defaultmap(none) map(to: A))
#pragma omp target map (from: _ompvv_isOffloadingOn)
