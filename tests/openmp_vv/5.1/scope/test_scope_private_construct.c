#pragma omp parallel shared(test_int)
#pragma omp scope private(test_int)
#pragma omp target map (from: _ompvv_isOffloadingOn)
