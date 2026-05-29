#pragma omp target data map(tofrom: compute_array)
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
