#pragma omp target parallel for num_threads(8) map(to: y, z) map(tofrom: x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
