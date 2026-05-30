#pragma omp target teams distribute parallel for map(to: a, b, scalar) map(tofrom: d)
#pragma omp target map (from: _ompvv_isOffloadingOn)
