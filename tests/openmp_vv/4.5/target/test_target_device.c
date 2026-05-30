#pragma omp target data map(tofrom: array[0:1000]) device(dev)
#pragma omp target map(alloc: array[0:1000]) device(dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
