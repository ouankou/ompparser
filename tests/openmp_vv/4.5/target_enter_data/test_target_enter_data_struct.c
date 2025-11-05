#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: single) map(to: array[0:5])
#pragma omp target map(from: singleCopy) map(from: arrayCopy[0:5]) map(tofrom: isHost) map(alloc: single, array[0:5])
#pragma omp target exit data map(delete: single, array[0:5])
#pragma omp target enter data map(to: single) map(to: array[0:5])
#pragma omp target map(from: singleCopy) map(from: arrayCopy[0:5]) map(tofrom: isHost) map(alloc: single, array[0:5])
#pragma omp target exit data map(delete: single, array[0:5])
#pragma omp target map (from: _ompvv_isOffloadingOn)
