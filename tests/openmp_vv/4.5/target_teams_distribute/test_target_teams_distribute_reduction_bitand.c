#pragma omp target teams distribute reduction(&:b) defaultmap(tofrom:scalar)
#pragma omp target map (from: _ompvv_isOffloadingOn)
