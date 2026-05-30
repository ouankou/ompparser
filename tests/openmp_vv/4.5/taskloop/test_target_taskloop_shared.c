#pragma omp target map(tofrom: s_val)
#pragma omp parallel
#pragma omp single
#pragma omp taskloop shared(s_val)
#pragma omp atomic update
#pragma omp target map (from: _ompvv_isOffloadingOn)
