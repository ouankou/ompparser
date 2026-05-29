#pragma omp parallel shared(s)
#pragma omp for
#pragma omp single
#pragma omp scope reduction(+:s)
#pragma omp target map (from: _ompvv_isOffloadingOn)
