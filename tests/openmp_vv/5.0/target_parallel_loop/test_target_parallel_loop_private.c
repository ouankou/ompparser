#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target parallel loop private(priv_val) num_threads(8)
