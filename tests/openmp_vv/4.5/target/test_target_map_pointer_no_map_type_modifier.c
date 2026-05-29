#pragma omp target map(p[0:1000])
#pragma omp target map (from: _ompvv_isOffloadingOn)
