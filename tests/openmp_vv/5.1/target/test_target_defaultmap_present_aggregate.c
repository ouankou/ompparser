#pragma omp target data map(tofrom: a, new_struct)
#pragma omp target map(tofrom: errors) defaultmap(present: aggregate)
#pragma omp target map (from: _ompvv_isOffloadingOn)
