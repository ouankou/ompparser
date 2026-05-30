#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target parallel num_threads(8) map(tofrom: a, b, c, d, num_threads)
#pragma omp loop private(privatized)
