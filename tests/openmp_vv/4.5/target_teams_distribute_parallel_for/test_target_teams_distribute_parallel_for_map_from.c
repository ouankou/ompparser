#pragma omp target teams distribute parallel for map(from: a, scalar)
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
