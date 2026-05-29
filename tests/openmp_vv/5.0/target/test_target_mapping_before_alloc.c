#pragma omp target map(alloc: scalar, a, member) map(to: scalar, a, member) map(tofrom: errors)
#pragma omp target map (from: _ompvv_isOffloadingOn)
