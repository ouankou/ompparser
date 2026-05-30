#pragma omp target data map(tofrom: x)
#pragma omp target map(present, to: x) map(from: x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
