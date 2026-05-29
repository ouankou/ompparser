#pragma omp target parallel num_threads(8) map(from:num_threads[0:8])
#pragma omp target map (from: _ompvv_isOffloadingOn)
