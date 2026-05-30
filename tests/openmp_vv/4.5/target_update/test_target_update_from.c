#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: a[:100], b[:100])
#pragma omp target
#pragma omp target update from(b[:100])
#pragma omp target
