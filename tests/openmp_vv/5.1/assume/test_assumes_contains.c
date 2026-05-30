#pragma omp assumes contains(target, parallel, for)
#pragma omp target map(tofrom: arr)
#pragma omp parallel num_threads(8)
#pragma omp for
#pragma omp target map (from: _ompvv_isOffloadingOn)
