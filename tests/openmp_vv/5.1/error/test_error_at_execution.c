#pragma omp parallel
#pragma omp single
#pragma omp error at(execution) severity(warning)
#pragma omp target map (from: _ompvv_isOffloadingOn)
