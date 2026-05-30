#pragma omp target map(tofrom: array[0:1000])
#pragma omp target map (from: _ompvv_isOffloadingOn)
