#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: compute_array[0:10000])
