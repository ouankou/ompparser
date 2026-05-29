#pragma omp target data map(tofrom: A, B) device(gpu)
#pragma omp target parallel for collapse(2) shared(A, B) device(gpu)
#pragma omp atomic
#pragma omp target map (from: _ompvv_isOffloadingOn)
