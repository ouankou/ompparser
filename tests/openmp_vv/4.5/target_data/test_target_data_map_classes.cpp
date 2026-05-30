#pragma omp target data map(from: array[0:1000]) map(tofrom: obj[0:1])
#pragma omp target
#pragma omp target data map(from: array[0:1000]) map(tofrom: obj)
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
