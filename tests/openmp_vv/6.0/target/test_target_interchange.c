#pragma omp target map(tofrom: a)
#pragma omp interchange
#pragma omp target map (from: _ompvv_isOffloadingOn)
