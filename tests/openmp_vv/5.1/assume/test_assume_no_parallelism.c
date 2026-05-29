#pragma omp assume no_parallelism
#pragma omp target parallel for map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
