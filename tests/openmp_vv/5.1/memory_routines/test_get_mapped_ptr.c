#pragma omp target enter data device(i) map(to:x)
#pragma omp target exit data map(from:x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
