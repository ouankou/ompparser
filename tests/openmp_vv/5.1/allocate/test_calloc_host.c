#pragma omp parallel for
#pragma omp atomic write
#pragma omp parallel for
#pragma omp parallel for
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
