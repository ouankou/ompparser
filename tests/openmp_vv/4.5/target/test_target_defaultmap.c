#pragma omp target defaultmap(tofrom: scalar)
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
