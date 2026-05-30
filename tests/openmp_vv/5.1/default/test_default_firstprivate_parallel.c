#pragma omp parallel default(firstprivate) num_threads(8)
#pragma omp target map (from: _ompvv_isOffloadingOn)
