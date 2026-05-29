#pragma omp parallel num_threads(8)
#pragma omp single
#pragma omp taskloop default(firstprivate)
#pragma omp target map (from: _ompvv_isOffloadingOn)
