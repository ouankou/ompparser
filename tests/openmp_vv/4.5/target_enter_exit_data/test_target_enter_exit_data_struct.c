#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: single) map(to: array[0:5])
#pragma omp target map(alloc: single) map(alloc: array[0:5])
#pragma omp target exit data map(from: single) map(from: array[0:5])
#pragma omp target enter data map(to: single) map(to: array[0:5])
#pragma omp target map(alloc: single) map(alloc: array[0:5])
#pragma omp target exit data map(from: single) map(from: array[0:5])
#pragma omp target map (from: _ompvv_isOffloadingOn)
