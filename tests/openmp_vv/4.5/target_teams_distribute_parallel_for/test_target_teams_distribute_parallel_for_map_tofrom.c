#pragma omp target teams distribute parallel for map(tofrom: a, b, c, d, scalar_to, scalar_from)
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
