#pragma omp target data map(tofrom: A[0:1024]) device(gpu)
#pragma omp target parallel for device(gpu)
#pragma omp target map (from: _ompvv_isOffloadingOn)
