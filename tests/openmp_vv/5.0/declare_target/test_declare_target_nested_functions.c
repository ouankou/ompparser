#pragma omp target map(tofrom: result)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp declare target to(outer_fn)
