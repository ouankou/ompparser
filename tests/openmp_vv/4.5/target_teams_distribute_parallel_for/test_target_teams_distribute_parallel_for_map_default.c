#pragma omp target teams distribute parallel for
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
