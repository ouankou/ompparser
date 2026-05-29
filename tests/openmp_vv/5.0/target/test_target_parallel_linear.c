#pragma omp target data map(tofrom: A[0:1024], val) device(gpu)
#pragma omp target parallel for device(gpu) linear(val:2)
#pragma omp target map (from: _ompvv_isOffloadingOn)
