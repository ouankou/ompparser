#pragma omp target parallel for lastprivate(conditional: x) map(tofrom: x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
