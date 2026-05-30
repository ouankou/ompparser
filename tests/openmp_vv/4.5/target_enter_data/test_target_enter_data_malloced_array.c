#pragma omp target enter data map(to:x[:n])
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(to: n) map(tofrom: B)
