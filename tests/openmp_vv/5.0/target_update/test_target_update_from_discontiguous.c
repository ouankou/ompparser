#pragma omp target data map(to: result[0:64])
#pragma omp target map(alloc: result[0:64])
#pragma omp target update from(result[0:64/2:2])
#pragma omp target map (from: _ompvv_isOffloadingOn)
