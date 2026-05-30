#pragma omp target map (close, tofrom: scalar, a, member)
#pragma omp target map (from: _ompvv_isOffloadingOn)
