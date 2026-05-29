#pragma omp target data map(tofrom: result[0:64])
#pragma omp target update to(result[0:64/2:2])
#pragma omp target map(alloc: result[0:64])
#pragma omp target map (from: _ompvv_isOffloadingOn)
