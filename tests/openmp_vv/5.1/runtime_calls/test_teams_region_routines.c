#pragma omp teams
#pragma omp parallel masked
#pragma omp target map (from: _ompvv_isOffloadingOn)
