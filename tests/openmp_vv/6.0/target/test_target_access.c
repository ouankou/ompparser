#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(array[0:16]) device(first_dev)
#pragma omp target map(array[0:16]) device(second_dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
