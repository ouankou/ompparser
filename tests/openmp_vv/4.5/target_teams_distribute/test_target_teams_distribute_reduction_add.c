#pragma omp target teams distribute reduction(+:total) defaultmap(tofrom:scalar)
#pragma omp target map (from: _ompvv_isOffloadingOn)
