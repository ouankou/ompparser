#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: on_init_dev, scalar, x)
