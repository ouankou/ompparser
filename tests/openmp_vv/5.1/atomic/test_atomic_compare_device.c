#pragma omp target parallel for map(pmax) shared(pmax)
#pragma omp atomic compare
#pragma omp target map (from: _ompvv_isOffloadingOn)
